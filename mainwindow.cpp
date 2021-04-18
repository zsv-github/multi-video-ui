#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "iostream"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButton_clicked()
{
    cv::Mat a = cv::imread("s", 1);
    cv::Mat image = cv::imread("../face-ui/test.png");
    if(!image.data) {
        std::cout << "ERRRORRRR";
        return;
    }
    cv::namedWindow("test", cv::WINDOW_AUTOSIZE);
    cv::imshow("test", image);

}
