#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "iostream"
#include <QTimer>
#include "grpc_server.h"

std::mutex mu;
std::string faissIndexDir = "../face_recognition/model";
FaceRecognizer faceRecognizer = FaceRecognizer(
        "../face_recognition/model/fd.onnx",
        "../face_recognition/model/fr.onnx"
);
FaissService faissService = FaissService(5000);

const int CAMERA_INDEX = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    std::cout << "constructor\n";
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
    std::thread first(GrpcServer);
    std::thread third(Recognize, std::ref(inputImgs), std::ref(resultFaceQueue));
    //TODO using gstreamer to reduce latency P[0]
    Timer timer;
    cv::Mat drawFrame, processImg;
    cv::VideoCapture capture;
    std::vector<ResultFace> resultFace;

    if(!capture.open("/mnt/hdd/CLionProjects/nist/face_recognition/test1.mp4"))
    {
        QMessageBox::critical(this,
                              "Video Error",
                              "Make sure you entered a correct and supported video file path,"
                              "<br>or a correct RTSP feed URL!");
        return;
    }

    while (true) {
        // TODO hUY break other threads as well
        if(close) {
            break;
        }
        bool success = capture.read(processImg);
        if (!success) {
            continue;
        }
        drawFrame = processImg.clone();
        if(inputImgs.size() < 2){
            inputImgs.push(processImg);
        }

        if(timer.elapsed() > 1){
            resultFace.clear();
        }

        if(!resultFaceQueue.empty()){
            resultFace = resultFaceQueue.front();
            timer.reset();
            resultFaceQueue.pop();
        }

        // TODO ---hUY LV ---- resultFace[i].img: crop faces
        for(int i = 0; i < resultFace.size(); i++){
            int alignImgSize = 448;
            cv::Mat face;
            cv::resize(resultFace[i].img, face, cv::Size(alignImgSize, alignImgSize));
            cv::Mat f = resultFace[i].img;
            QImage q = QImage(f.data, f.cols, f.rows, f.step, QImage::Format_RGB888);
//            ui->detectedImage->setPixmap(QPixmap::fromImage(q.rgbSwapped()));
            QString a = QString::fromStdString("ID: " + std::to_string(resultFace[i].id));
//            ui->detectedName->setText(a);
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
