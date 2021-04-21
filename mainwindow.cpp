#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "iostream"
#include <QTimer>

#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <opencv2/opencv.hpp>
#include "multiple_camera_server.grpc.pb.h"
#include "multiple_camera_server.pb.h"
#include "face_recognition.h"
#include "boost/filesystem.hpp"
#include "faiss_service.h"
#ifdef USE_JETSON_UTILS
#include "videoSource.h"
#include "videoOutput.h"
#endif


using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using multiple_camera_server::FaceProcessing;
using multiple_camera_server::RegisterFaceRequest;
using multiple_camera_server::RegisterFaceResponse;
using idx_t = faiss::Index::idx_t;


class FaceProcessingServiceImpl final : public FaceProcessing::Service {
public:
    explicit FaceProcessingServiceImpl(std::queue<bool> &notifier) {
        this->notifier = &notifier;
        faissIndexDir = "../face_recognition/model";
        faceRecognizer = new FaceRecognizer(
                "../face_recognition/model/fd.onnx",
                "../face_recognition/model/fr.onnx"
        );
        faissService = new FaissService(5000);
        if (boost::filesystem::exists(faissIndexDir + "/" + faissService->faissIndexFileName)) {
            faissService->LoadIndex(faissIndexDir);
        }
        printf("Total vector in file: %ld\n", this->faissService->index->ntotal);
    }

    ~FaceProcessingServiceImpl() override {
        delete faceRecognizer;
        delete faissService;
    };

    Status
    RegisterFaces(ServerContext *context, const RegisterFaceRequest *request, RegisterFaceResponse *reply) override {
        const std::string &imgData = request->image_bytes();
        std::vector<std::uint8_t> vectorData(imgData.begin(), imgData.end());
        cv::Mat dataMat(vectorData, true);
        cv::Mat registerImage(cv::imdecode(dataMat, 1));

        printf("Register an image with size: %d, %d\n", registerImage.cols, registerImage.rows);

        std::vector<bbox> boxes;
        cv::Mat processImg = registerImage.clone();
        this->faceRecognizer->Detect(processImg, boxes);
        printf("Found %zu faces on image\n", boxes.size());

        if (boxes.empty()) {
            reply->set_face_id(-1);
            return Status::OK;
        }
        std::vector<cv::Point2f> landmarks;
        int max_area = 0;
        bbox maxAreaBox{};
        for (auto &box: boxes) {
            cv::Rect rect = cv::Rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
            int temp_area = rect.area();
            if (temp_area > max_area) {
                max_area = temp_area;
                maxAreaBox = box;
            }
        }
        if (max_area > 0) {
            for (auto &point:maxAreaBox.point) {
                landmarks.emplace_back(cv::Point2f(point._x, point._y));
            }
        }
        cv::Mat alignedFace;
        Aligner aligner;
        aligner.AlignFace(registerImage, landmarks, &alignedFace);
        // cv::imwrite("crop_register.png", alignedFace);
        std::vector<float> featureVector;
        featureVector = faceRecognizer->Recognize(alignedFace);
        printf("Get feature vector with size %zu\n", featureVector.size());
        std::vector<long> ids(1);
        ids[0] = rand() % 1000000;
        printf("Using id: %ld for registration\n", ids[0]);
        faissService->FaissInsertWithIds(1, featureVector.data(), ids);
        faissService->SaveIndex2File(faissIndexDir);
        reply->set_face_id(ids[0]);
        notifier->push(true);
        return Status::OK;
    }

private:
    FaceRecognizer *faceRecognizer;
    FaissService *faissService;
    std::string faissIndexDir;
    std::queue<bool> *notifier;
};

void GrpcServer(std::queue<bool> &notifier) {
    std::string server_address("0.0.0.0:50051");
    FaceProcessingServiceImpl service(notifier);
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    server->Wait();
}

[[noreturn]] void Recognize(std::queue<bool> &notifier, std::queue<cv::Mat> &inputImgs, std::queue<std::vector<ResultFace>> &resultFacesQueue){
    std::string faissIndexDir = "../face_recognition/model";
    FaceRecognizer faceRecognizer = FaceRecognizer(
            "../face_recognition/model/fd.onnx",
            "../face_recognition/model/fr.onnx"
    );
    FaissService faissService = FaissService(5000);
    if (boost::filesystem::exists(faissIndexDir + "/" + faissService.faissIndexFileName)) {
        faissService.LoadIndex(faissIndexDir);
    }
    printf("Total vector in file: %ld\n", faissService.index->ntotal);
    float distance_threshold = 0.66;
    float confidence_threshold = 80;
    Timer t1, t2;
    cv::Mat originImg, processImg;

    while (true){
        if (!notifier.empty()) {
            faissService.LoadIndex(faissIndexDir);
            printf("Reload index file\n");
            notifier.pop();
        }
        if(!inputImgs.empty()){
            processImg = inputImgs.front();
            originImg = processImg.clone();
            inputImgs.pop();
            // TODO safe reference for processImg
            std::vector<bbox> boxes;
            faceRecognizer.Detect(processImg, boxes);
            t1.out("Detect faces");
            // printf("Found %zu faces on image\n", boxes.size());
            std::vector<ResultFace> resultFaces;
            for (auto &box: boxes) {
                if(box.x1 < 0) box.x1 = 0;
                if(box.y1 < 0) box.y1 = 0;
                if(box.x2 > originImg.cols) box.x2 = originImg.cols;
                if(box.y2 > originImg.rows) box.y2 = originImg.rows;
                cv::Rect rect = cv::Rect(box.x1, box.y1, box.x2 - box.x1, box.y2 - box.y1);
                t1.reset();
                std::vector<cv::Point2f> landmarks;
                for (auto &point: box.point) {
                    landmarks.emplace_back(cv::Point2f(point._x, point._y));
                }
                cv::Mat alignedFace;
                // TODO consider adding Tracking for higher accuracy

                // TODO make a better aligner P[1]
                Aligner aligner;
                aligner.AlignFace(originImg, landmarks, &alignedFace);
                t1.out("align face");
                // cv::imwrite("img.png", alignedFace);
                std::vector<float> featureVector;
                featureVector = faceRecognizer.Recognize(alignedFace);
                t1.out("extract feature");
                std::vector<float> distances;
                std::vector<faiss::Index::idx_t> indexes;

                // TODO search for multiple vectors at once [P0]
                faissService.FaissSearch(featureVector.data(), 5, distances, indexes);
                float distance = distances[0];
                t1.out("faiss search");
                auto confidence = float(fmin(100, 100 * distance_threshold / distance));
                int index = indexes[0];
                if (confidence > confidence_threshold) {
                    printf("found index: %d\n", index);
                    printf("confidence: %f\n", confidence);
                    // TODO separate display sector and make a better visualization P[0]
                    cv::Mat face =  originImg(rect);
                    resultFaces.push_back({face, index});
                    // cv::rectangle(drawFrame, rect, cv::Scalar(0, 0, 255));
                }

            }
            if(!resultFaces.empty()){
                // TODO: Check queue's size < pre-defined number, adapt to low computing power
                resultFacesQueue.push(resultFaces);
            }
        }
//        t2.out("total time");
    }
}

const int CAMERA_INDEX = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    std::cout << "constructor";
    ui->setupUi(this);

    close = false;
    ui->graphicsView->setScene(new QGraphicsScene(this));
    ui->graphicsView->scene()->addItem(&pixmap);
}

void MainWindow::showEvent( QShowEvent* event ) {
    QMainWindow::showEvent( event );
    QTimer::singleShot(0, this, SLOT(startVideo()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::startVideo()
{
    std::queue<cv::Mat> inputImgs;
    std::queue<std::vector<ResultFace>> resultFaceQueue;
    std::queue<bool> notifier;
    std::thread first(GrpcServer, std::ref(notifier));
    std::thread third(Recognize, std::ref(notifier), std::ref(inputImgs), std::ref(resultFaceQueue));
    //TODO using gstreamer to reduce latency P[0]
    Timer t0, t1, t2;
    cv::Mat drawFrame, processImg;
    cv::VideoCapture capture(0);
    std::vector<ResultFace> resultFace;

    if(!capture.open(CAMERA_INDEX))
    {
        QMessageBox::critical(this,
                              "Video Error",
                              "Make sure you entered a correct and supported video file path,"
                              "<br>or a correct RTSP feed URL!");
        return;
    }

    while (true) {
        if(close) {
            break;
        }
        t1.reset();
        bool success = capture.read(processImg);
        if (!success) {
            continue;
        }
        drawFrame = processImg.clone();
        processImg = processImg.clone();
        t1.out("Read image");

        // TODO: Check queue's size < pre-defined number, remove old frames, adapt to low computing power
        if(t0.elapsed() > 0.5){
            inputImgs.push(processImg);
            // resultFace.clear();
            t0.reset();
        }

        if(t2.elapsed() > 1){
            resultFace.clear();
        }

        if(!resultFaceQueue.empty()){
            resultFace = resultFaceQueue.front();
            t2.reset();
            resultFaceQueue.pop();
        }

        // TODO ---hUY LV ---- resultFace[i].img: crop faces
        for(int i = 0; i < resultFace.size(); i++){
            int alignImgSize = 448;
            cv::Mat face;
            cv::resize(resultFace[i].img, face, cv::Size(alignImgSize, alignImgSize));
            cv::Mat f = resultFace[i].img;
            QImage q = QImage(f.data, f.cols, f.rows, f.step, QImage::Format_RGB888);
            ui->detectedImage->setPixmap(QPixmap::fromImage(q.rgbSwapped()));
            QString a = QString::fromStdString("ID: " + std::to_string(resultFace[i].id));
            ui->detectedName->setText(a);
            cv::Mat insertImg(drawFrame, cv::Rect(0, i*alignImgSize, alignImgSize, alignImgSize));
            face.copyTo(insertImg);
            cv::putText(drawFrame,
                        std::to_string(resultFace[i].id),
                        cv::Point(alignImgSize, (i+1)*alignImgSize),
                        cv::FONT_HERSHEY_DUPLEX,
                        2,
                        CV_RGB(255, 0, 0),
                        2);

        }
        copyMakeBorder(drawFrame,
                       drawFrame,
                       1,1,1,1,
                       cv::BORDER_CONSTANT);

        QImage qimg(drawFrame.data,
                    drawFrame.cols,
                    drawFrame.rows,
                    drawFrame.step,
                    QImage::Format_RGB888);
        pixmap.setPixmap( QPixmap::fromImage(qimg.rgbSwapped()) );
        ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);
        qApp->processEvents();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    close = true;
    event->accept();
}
