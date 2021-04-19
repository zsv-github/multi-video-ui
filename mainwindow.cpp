#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "iostream"
#include <QTimer>

using namespace cv;
using namespace std;
const int CAMERA_INDEX = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    cout << "constructor";
    ui->setupUi(this);

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
    if(video.isOpened())
    {
        ui->startButton->setText("Start");
        video.release();
        return;
    }

    if(!video.open(CAMERA_INDEX))
    {
        QMessageBox::critical(this,
                              "Video Error",
                              "Make sure you entered a correct and supported video file path,"
                              "<br>or a correct RTSP feed URL!");
        return;
    }

    ui->startButton->setText("Stop");

    Mat frame;
    while(video.isOpened())
    {
        video >> frame;
        if(!frame.empty())
        {
            copyMakeBorder(frame,
                           frame,
                           1,1,1,1,
                           BORDER_CONSTANT);

            QImage qimg(frame.data,
                        frame.cols,
                        frame.rows,
                        frame.step,
                        QImage::Format_RGB888);
            pixmap.setPixmap( QPixmap::fromImage(qimg.rgbSwapped()) );
            ui->graphicsView->fitInView(&pixmap, Qt::KeepAspectRatio);
        }
        qApp->processEvents();
    }

    ui->startButton->setText("Start");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    video.release();
    event->accept();
}
