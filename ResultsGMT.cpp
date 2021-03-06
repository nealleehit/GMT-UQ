/* *****************************************************************************
Copyright (c) 2016-2017, The Regents of the University of California (Regents).
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies,
either expressed or implied, of the FreeBSD Project.

REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED HEREUNDER IS 
PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, 
UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

*************************************************************************** */

// Written: fmckenna

#include "ResultsGMT.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>

#include <QTabWidget>
#include <QTextEdit>
#include <MyTableWidget.h>
#include <QDebug>
#include <QHBoxLayout>
#include <QColor>

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#include <QMessageBox>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QVXYModelMapper>
using namespace QtCharts;
#include <math.h>
#include <QValueAxis>
#include <QXYSeries>
#include <QLabel>
#include <QDirIterator>
#include <QFileInfo>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <EarthquakeRecord.h>

#include <ResponseWidget.h>
#include <timeIntegrators.h>

int CalcResponseSpectrum(const QVector<double> &periods,
                         double dampingRatio,
                         const char *integrator,
                         const std::vector<double> &groundMotion,
                         double time_step,
                         QVector<double> &dispResponse,
                         QVector<double> &accelResponse);

#define NUM_DIVISIONS 10


ResultsGMT::ResultsGMT(QWidget *parent)
    : SimCenterAppWidget(parent)
{
    layout = new QVBoxLayout();

    // title & add button
    tabWidget = new QTabWidget(this);
    layout->addWidget(tabWidget,1);
    mLeft = true;
    col1 = 0;
    col2 = 0;
}

ResultsGMT::~ResultsGMT()
{

}


void ResultsGMT::clear(void)
{
  //
  // get the tab widgets and delete them
  //

    QWidget *res=tabWidget->widget(0);
    QWidget *gen=tabWidget->widget(1);
    QWidget *dat=tabWidget->widget(2);

    delete dat;
    delete gen;
    delete res;

    tabWidget->clear();

    //
    // clear any data we have stored
    // 

    theHeadings.clear();
    theNames.clear();
    theMeans.clear();
    theStdDevs.clear();
    
}



bool
ResultsGMT::outputToJSON(QJsonObject &jsonObject)
{
    bool result = true;

    int numEDP = theNames.count();

    // quick return .. noEDP -> no analysis done -> no results out
    if (numEDP == 0)
      return true;

    jsonObject["resultType"]=QString(tr("ResultsGMT"));

    //
    // add summary data
    //

    QJsonArray resultsData;
    for (int i=0; i<numEDP; i++) {
        QJsonObject edpData;
        edpData["name"]=theNames.at(i);
        edpData["mean"]=theMeans.at(i);
        edpData["stdDev"]=theStdDevs.at(i);
        resultsData.append(edpData);
    }
    jsonObject["summary"]=resultsData;
    jsonObject["dataType"]=dataType;


    // add general data
    jsonObject["general"]=dakotaText->toPlainText();

    //
    // add spreadsheet data
    //

    QJsonObject spreadsheetData;

    int numCol = spreadsheet->columnCount();
    int numRow = spreadsheet->rowCount();

    spreadsheetData["numRow"]=numRow;
    spreadsheetData["numCol"]=numCol;

    QJsonArray headingsArray;
    for (int i = 0; i <theHeadings.size(); ++i) {
        headingsArray.append(QJsonValue(theHeadings.at(i)));
    }

    spreadsheetData["headings"]=headingsArray;

    QJsonArray dataArray;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (int row = 0; row < numRow; ++row) {
        for (int column = 0; column < numCol; ++column) {
            QTableWidgetItem *theItem = spreadsheet->item(row,column);
            QString textData = theItem->text();
            dataArray.append(textData.toDouble());
        }
    }
    QApplication::restoreOverrideCursor();
    spreadsheetData["data"]=dataArray;

    jsonObject["spreadsheet"] = spreadsheetData;
    return result;
}


bool
ResultsGMT::inputFromJSON(QJsonObject &jsonObject)
{
    bool result = true;

    this->clear();

    //
    // create a summary widget in which place basic output (name, mean, stdDev)
    //

    QWidget *summaryWidget = new QWidget();
    QVBoxLayout *summaryLayout = new QVBoxLayout();


    QJsonArray edpArray = jsonObject["summary"].toArray();
    QJsonValue type = jsonObject["dataType"];
    if (!type.isNull()) {
        dataType = type.toInt();
    } else
        dataType = 0;

    foreach (const QJsonValue &edpValue, edpArray) {
        QString name;
        double mean, stdDev;
        QJsonObject edpObject = edpValue.toObject();
        QJsonValue theNameValue = edpObject["name"];
        name = theNameValue.toString();

        QJsonValue theMeanValue = edpObject["mean"];
        mean = theMeanValue.toDouble();

        QJsonValue theStdDevValue = edpObject["stdDev"];
        stdDev = theStdDevValue.toDouble();

        QWidget *theWidget = this->createResultEDPWidget(name, mean, stdDev, dataType);
        summaryLayout->addWidget(theWidget);
    }
    summaryLayout->addStretch();
    summaryWidget->setLayout(summaryLayout);

    //
    // place widget in scrollable area
    //

    QScrollArea *summary = new QScrollArea;
    summary->setWidgetResizable(true);
    summary->setLineWidth(0);
    summary->setFrameShape(QFrame::NoFrame);
    summary->setWidget(summaryWidget);

    //
    // into a QTextEdit place more detailed Dakota text
    //

    dakotaText = new QTextEdit();
    dakotaText->setReadOnly(true); // make it so user cannot edit the contents
    QJsonValue theValue = jsonObject["general"];
    dakotaText->setText(theValue.toString());

    //
    // into a spreadsheet place all the data returned
    //

    spreadsheet = new MyTableWidget();
    QJsonObject spreadsheetData = jsonObject["spreadsheet"].toObject();
    int numRow = spreadsheetData["numRow"].toInt();
    int numCol = spreadsheetData["numCol"].toInt();
    spreadsheet->setColumnCount(numCol);
    spreadsheet->setRowCount(numRow);

    QJsonArray headingData= spreadsheetData["headings"].toArray();
    for (int i=0; i<numCol; i++) {
        theHeadings << headingData.at(i).toString();
    }

    spreadsheet->setHorizontalHeaderLabels(theHeadings);

    QJsonArray dataData= spreadsheetData["data"].toArray();
    int dataCount =0;
    for (int row =0; row<numRow; row++) {
        for (int col=0; col<numCol; col++) {
            QModelIndex index = spreadsheet->model()->index(row, col);
            spreadsheet->model()->setData(index, dataData.at(dataCount).toDouble());
            dataCount++;
        }
    }
    spreadsheet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(spreadsheet,SIGNAL(cellPressed(int,int)),this,SLOT(onSpreadsheetCellClicked(int,int)));

    //
    // create a chart, setting data points from first and last col of spreadsheet
    //

    chart = new QChart();
    chart->setAnimationOptions(QChart::AllAnimations);
    QScatterSeries *series = new QScatterSeries;
    col1 = 0;
    col2 = numCol-1;
    mLeft = true;

    this->onSpreadsheetCellClicked(0,numCol-1);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->chart()->legend()->hide();

    //
    // create a widget into which we place the chart and the spreadsheet
    //

    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addWidget(chartView, 1);
    layout->addWidget(spreadsheet, 1);

    //
    // add 3 Widgets to TabWidget
    //

    tabWidget->addTab(theGraphic, tr("Response Spectrum"));
    tabWidget->addTab(summary,tr("Summary PGA"));
    tabWidget->addTab(widget, tr("PGA Values"));
    tabWidget->adjustSize();

    return result;
}


static void merge_helper(double *input, int left, int right, double *scratch)
{
    // if one element: done  else: recursive call and then merge
    if(right == left + 1) {
        return;
    } else {
        int length = right - left;
        int midpoint_distance = length/2;
        /* l and r are to the positions in the left and right subarrays */
        int l = left, r = left + midpoint_distance;

        // sort each subarray
        merge_helper(input, left, left + midpoint_distance, scratch);
        merge_helper(input, left + midpoint_distance, right, scratch);

        // merge the arrays together using scratch for temporary storage
        for(int i = 0; i < length; i++) {
            /* Check to see if any elements remain in the left array; if so,
            * we check if there are any elements left in the right array; if
            * so, we compare them.  Otherwise, we know that the merge must
            * use take the element from the left array */
            if(l < left + midpoint_distance &&
                    (r == right || fmin(input[l], input[r]) == input[l])) {
                scratch[i] = input[l];
                l++;
            } else {
                scratch[i] = input[r];
                r++;
            }
        }
        // Copy the sorted subarray back to the input
        for(int i = left; i < right; i++) {
            input[i] = scratch[i - left];
        }
    }
}

static int mergesort(double *input, int size)
{
    double *scratch = new double[size];
    if(scratch != NULL) {
        merge_helper(input, 0, size, scratch);
        delete [] scratch;
        return 1;
    } else {
        return 0;
    }
}

int ResultsGMT::processResults(QString filenameResults, QString filenameTab, QString inputFile) {

    qDebug() << "Processing Results.." << filenameTab;

    this->clear();

    mLeft = true;
    col1 = 0;
    col2 = 0;

    //
    // get a Qwidget ready to place summary data, the EDP name, mean, stdDev into
    //
    QString xLabel("Period");
    QString yLabel("Displacement");
    theGraphic = new ResponseWidget(xLabel, yLabel);

    QWidget *summaryWidget = new QWidget();
    QVBoxLayout *summaryLayout = new QVBoxLayout();
    summaryWidget->setLayout(summaryLayout);

    //
    // into a QTextEdit we will place contents of Dakota more detailed output
    //

    dakotaText = new QTextEdit();
    dakotaText->setReadOnly(true); // make it so user cannot edit the contents
    dakotaText->setText("\n");

    //
    // open Dakota output file
    //

    std::ifstream fileResults(filenameResults.toStdString().c_str());
    if (!fileResults.is_open()) {
        emit sendErrorMessage( QString("Could not open file: ") + filenameResults + QString(" Dakota did not start. Check error file dakota.err in local directory or at DesignSafe"));
        return -1;
    }

    // now ignore every line until Statistics found

    const std::string needle = "Statistics";
    std::string haystack;

    //
    // parse till get to Statistics output
    //
    int statisticsFound = 0;
    while (std::getline(fileResults, haystack)) {
      if (haystack.find(needle) != std::string::npos) {
	  statisticsFound = 1;
            break;
        }
    }
    
    if (statisticsFound == 0) {
      emit sendErrorMessage(tr("ERROR: Dakota Failed to finish. Look in  dakota.err locally or at job archive on DesignSafe"));
      return -1;
    } else {
      emit sendErrorMessage(tr("UQ Sampling Results"));
    }

    //
    // now copy line and every subsequent line into text editor
    //  - also if still collecting sumary data, add each EDP's sumary info
    //

    dakotaText->append(haystack.c_str());

    //
    // get 2 lines to get to summary
    //
    std::getline(fileResults, haystack); dakotaText->append(haystack.c_str());
    std::getline(fileResults, haystack); dakotaText->append(haystack.c_str());

    //
    // switch on summary (mean/stdDev or Min/Max
    //

    bool minAndMax = true;
    const std::string nextNeedle = "Sample";
    if (haystack.find(nextNeedle) != std::string::npos) {
        minAndMax = false;
        std::getline(fileResults, haystack); dakotaText->append(haystack.c_str());
    }

    bool isSummaryDone = false;

    while (std::getline(fileResults, haystack)) {
        dakotaText->append(haystack.c_str());
        if (isSummaryDone == false) {

            if ( strlen(haystack.c_str()) == 0) {
                isSummaryDone = true;
            } else {



                std::istringstream iss(haystack);
                std::string subs;

                iss >> subs;
                QString  nameString(QString::fromStdString(subs));

                if (minAndMax == false) {

                    //
                    // add mean/stdDev
                    //

                    iss >> subs;
                    QString meanText(QString::fromStdString(subs));
                    double mean = meanText.toDouble();

                    iss >> subs;
                    QString stdDevText(QString::fromStdString(subs));
                    double stdDev = stdDevText.toDouble();

                    QWidget *theWidget = this->createResultEDPWidget(nameString, mean, stdDev, 0);
                    summaryLayout->addWidget(theWidget);

                } else {

                    //
                    // add min/max
                    //

                    iss >> subs;
                    iss >> subs;
                    iss >> subs;
                    QString minText(QString::fromStdString(subs));
                    double min = minText.toDouble();

                    iss >> subs;
                    iss >> subs;
                    iss >> subs;

                    QString maxText(QString::fromStdString(subs));
                    double max = maxText.toDouble();

                    QWidget *theWidget = this->createResultEDPWidget(nameString, min, max, 1);
                    summaryLayout->addWidget(theWidget);

                }
            }
        }
    }
    summaryLayout->addStretch();

    QScrollArea *summary = new QScrollArea;
    summary->setWidgetResizable(true);
    summary->setLineWidth(0);
    summary->setFrameShape(QFrame::NoFrame);
    summary->setWidget(summaryWidget);

    // close input file
    fileResults.close();

    //
    // now into a QTableWidget copy the random variable and edp's of each black box run
    //

    spreadsheet = new MyTableWidget();

    // open file containing tab data
    std::ifstream tabResults(filenameTab.toStdString().c_str());
    if (!tabResults.is_open()) {
        qDebug() << "Could not open file";
        return -1;
    }

    //
    // read first line and set headings (ignoring second column for now)
    //

    std::string inputLine;
    std::getline(tabResults, inputLine);
    std::istringstream iss(inputLine);
    int colCount = 0;
    theHeadings << "Run #";
    do
    {
        std::string subs;
        iss >> subs;
        qDebug() << QString(subs.c_str());
        if (colCount > 1) {
            if (subs != "" && subs != " ") {
                theHeadings << subs.c_str();
            }
        }
        colCount++;
    } while (iss);

    qDebug() << "SETTINGS: " << theHeadings << " " << theHeadings.count();

    colCount = theHeadings.count();
    spreadsheet->setColumnCount(colCount);
    spreadsheet->setHorizontalHeaderLabels(theHeadings);

    // now until end of file, read lines and place data into spreadsheet
    int rowCount = 0;
    while (std::getline(tabResults, inputLine)) {
        std::istringstream is(inputLine);
        int col=0;
        spreadsheet->insertRow(rowCount);
        for (int i=0; i<colCount+2; i++) {
            std::string data;
            is >> data;
            if (i != 1) {
                QModelIndex index = spreadsheet->model()->index(rowCount, col);
                spreadsheet->model()->setData(index, data.c_str());
                col++;
            }
        }
        rowCount++;
    }
    tabResults.close();

    if (rowCount == 0) {
      emit sendErrorMessage("Dakota FAILED to RUN Correctly");
      return -2;
    }
   // rowCount;
    spreadsheet->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(spreadsheet,SIGNAL(cellPressed(int,int)),this,SLOT(onSpreadsheetCellClicked(int,int)));

    //
    // create a chart, setting data points from first and last col of spreadsheet
    //

    chart = new QChart();
    chart->setAnimationOptions(QChart::AllAnimations);

    this->onSpreadsheetCellClicked(0,colCount-1);

    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->chart()->legend()->hide();

    //
    // into QWidget place chart and spreadsheet
    //

    QWidget *widget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->addWidget(chartView, 1);
    layout->addWidget(spreadsheet, 1);

    //
    // add summary, detained info and spreadsheet with chart to the tabed widget
    //
    // fmk THIS ONE
    tabWidget->addTab(summary,tr("Summary"));
    tabWidget->addTab(theGraphic, tr("Respose Spectrum"));
    tabWidget->addTab(widget, tr("PGA"));

    tabWidget->adjustSize();

    emit sendErrorMessage(tr(""));

    qDebug() << "looking at Results dir";
    QFileInfo tabFile(filenameTab);

    //QString fileDirectory = workDir.absoluteFilePath(filenameTab);
    QString resultsDirectory = tabFile.dir().absolutePath() + QDir::separator() + QString("Results");
    qDebug() << "looking at Results dir" << resultsDirectory;

    QDirIterator it(resultsDirectory, QStringList() << "*.json");
    while (it.hasNext()) {
         QString resultFile = it.next();
         this->addEarthquakeMotion(resultFile);
    }


    return 0;
}

void
ResultsGMT::getColData(QVector<double> &data, int numRow, int col) {
    bool ok;
    double data0 = spreadsheet->item(0,col)->text().toDouble(&ok);
    if (ok == true) {
        for (int i=0; i<numRow; i++) {
            QTableWidgetItem *item = spreadsheet->item(i,col);
            data.append(item->text().toDouble());
        }
    } else { // it's a string create a map
         QMap<QString, int> map;
         int numDifferent = 1;
         for (int i=0; i<numRow; i++) {
             QTableWidgetItem *item = spreadsheet->item(i,col);
             QString text = item->text();
             if (map.contains(text))
                 data.append(map.value(text));
             else {
                 data.append(numDifferent);
                 map[text] = numDifferent++;
             }
         }
    }
    return;
}

void
ResultsGMT::onSpreadsheetCellClicked(int row, int col)
{
    qDebug() << "onSPreadSheetCellClicked() :" << row << " " << col;
    mLeft = spreadsheet->wasLeftKeyPressed();

    // create a new series
    chart->removeAllSeries();
    //chart->removeA
    QAbstractAxis *oldAxisX=chart->axisX();
    if (oldAxisX != 0)
        chart->removeAxis(oldAxisX);
    QAbstractAxis *oldAxisY=chart->axisY();
    if (oldAxisY != 0)
        chart->removeAxis(oldAxisY);


    // QScatterSeries *series;//= new QScatterSeries;

    int oldCol = 0;
    if (mLeft == true) {
        oldCol= col2;
        col2 = col;
    } else {
        oldCol= col1;
        col1 = col;
    }

    int rowCount = spreadsheet->rowCount();


    if (col1 != col2) {

        QScatterSeries *series = new QScatterSeries;
        double minX, minY, maxX, maxY;

        QVector<double> dataX;
        QVector<double> dataY;
        this->getColData(dataX, rowCount, col1);
        this->getColData(dataY, rowCount, col2);
        for (int i=0; i<rowCount; i++) {
            QTableWidgetItem *itemX = spreadsheet->item(i,col1);
            QTableWidgetItem *itemY = spreadsheet->item(i,col2);
            QTableWidgetItem *itemOld = spreadsheet->item(i,oldCol);
            itemOld->setData(Qt::BackgroundRole, QColor(Qt::white));
            itemX->setData(Qt::BackgroundRole, QColor(Qt::lightGray));
            itemY->setData(Qt::BackgroundRole, QColor(Qt::lightGray));
	    
	    double valX = dataX[i];
	    double valY = dataY[i];
	    if (i == 0) {
	      minX = valX; maxX = valX; minY = valY; maxY = valY;
	    } else {
	      if (valX < minX) {
		minX = valX;
	      } else if (valX > maxX) {
		maxX = valX;
	      }
	      if (valY < minY) {
		minY = valY;
	      } else if (valY > maxY) {
		maxY = valY;
	      }
	    }

            series->append(valX, valY);
        }

        chart->addSeries(series);
        QValueAxis *axisX = new QValueAxis();
        QValueAxis *axisY = new QValueAxis();
        double xRange=maxX-minX;
        double yRange=maxY-minY;
	axisX->setRange(minX - 0.01*xRange, maxX + 0.1*xRange);
        axisY->setRange(minY - 0.1*yRange, maxY + 0.1*yRange);

        axisX->setTitleText(theHeadings.at(col1));
        axisY->setTitleText(theHeadings.at(col2));

        chart->setAxisX(axisX, series);
        chart->setAxisY(axisY, series);

    } else {

        QVector<double> dataX;
        this->getColData(dataX, rowCount, col1);

        QLineSeries *series= new QLineSeries;

        static double NUM_DIVISIONS_FOR_DIVISION = 10.0;
        double *dataValues = new double[rowCount];
        double histogram[NUM_DIVISIONS];
        for (int i=0; i<NUM_DIVISIONS; i++)
            histogram[i] = 0;

        double min = 0;
        double max = 0;
        for (int i=0; i<rowCount; i++) {
            QTableWidgetItem *itemX = spreadsheet->item(i,col1);
            QTableWidgetItem *itemOld = spreadsheet->item(i,oldCol);
            itemOld->setData(Qt::BackgroundRole, QColor(Qt::white));
            itemX->setData(Qt::BackgroundRole, QColor(Qt::lightGray));
            double value = dataX[i];
            dataValues[i] =  value;

            if (i == 0) {
                min = value;
                max = value;
            } else if (value < min) {
                min = value;
            } else if (value > max) {
                max = value;
            }
        }
        if (mLeft == true) {

            // frequency distribution
            double range = max-min;
            double dRange = range/NUM_DIVISIONS_FOR_DIVISION;

            for (int i=0; i<rowCount; i++) {
                // compute block belongs to, watch under and overflow due to numerics
                int block = floor((dataValues[i]-min)/dRange);
                if (block < 0) block = 0;
                if (block > NUM_DIVISIONS-1) block = NUM_DIVISIONS-1;
                histogram[block] += 1;
            }

            double maxPercent = 0;
            for (int i=0; i<NUM_DIVISIONS; i++) {
                histogram[i]/rowCount;
                if (histogram[i] > maxPercent)
                    maxPercent = histogram[i];
            }
            for (int i=0; i<NUM_DIVISIONS; i++) {
                series->append(min+i*dRange, 0);
                series->append(min+i*dRange, histogram[i]);
                series->append(min+(i+1)*dRange, histogram[i]);
                series->append(min+(i+1)*dRange, 0);
            }

            delete [] dataValues;

            chart->addSeries(series);
            QValueAxis *axisX = new QValueAxis();
            QValueAxis *axisY = new QValueAxis();

            axisX->setRange(min, max);
            axisY->setRange(0, maxPercent);
            axisY->setTitleText("Frequency %");
            axisX->setTitleText(theHeadings.at(col1));
            axisX->setTickCount(NUM_DIVISIONS+1);
            chart->setAxisX(axisX, series);
            chart->setAxisY(axisY, series);
    } else {

            // cumulative distributionn
            mergesort(dataValues, rowCount);

            for (int i=0; i<rowCount; i++) {
                series->append(dataValues[i], 1.0*i/rowCount);
            }

            delete [] dataValues;

            chart->addSeries(series);
            QValueAxis *axisX = new QValueAxis();
            QValueAxis *axisY = new QValueAxis();

            axisX->setRange(min, max);
            axisY->setRange(0, 1);
            axisY->setTitleText("Cumulative Probability");
            axisX->setTitleText(theHeadings.at(col1));
            axisX->setTickCount(NUM_DIVISIONS+1);
            chart->setAxisX(axisX, series);
            chart->setAxisY(axisY, series);
        }


    }
}


static QWidget *addLabeledLineEdit(QString theLabelName, QLineEdit **theLineEdit){

    QVBoxLayout *theLayout = new QVBoxLayout();
    QLabel *theLabel = new QLabel();
    theLabel->setText(theLabelName);
    QLineEdit *theEdit = new QLineEdit();
    theLabel->setMaximumWidth(200);
    theLabel->setMinimumWidth(200);
    theEdit->setMaximumWidth(200);
    theEdit->setMinimumWidth(200);
    theLayout->addWidget(theLabel);
    theLayout->addWidget(theEdit);
    theLayout->setSpacing(0);
    theLayout->setMargin(0);

    QWidget *theWidget = new QWidget();
    theWidget->setLayout(theLayout);

    *theLineEdit = theEdit;
    return theWidget;
}

QWidget *
ResultsGMT::createResultEDPWidget(QString &name, double mean, double stdDev, int valueType) {

    dataType = valueType;

    QWidget *edp = new QWidget;
    QHBoxLayout *edpLayout = new QHBoxLayout();

    edp->setLayout(edpLayout);

    QLineEdit *nameLineEdit;
    QWidget *nameWidget = addLabeledLineEdit(QString("Name"), &nameLineEdit);
    nameLineEdit->setText(name);
    nameLineEdit->setDisabled(true);
    theNames.append(name);
    edpLayout->addWidget(nameWidget);

    QLineEdit *firstLineEdit;
    QWidget *firstWidget;
    if (valueType == 0)
       firstWidget = addLabeledLineEdit(QString("Mean"), &firstLineEdit);
    else
       firstWidget = addLabeledLineEdit(QString("Min"), &firstLineEdit);


    firstLineEdit->setText(QString::number(mean));
    firstLineEdit->setDisabled(true);
    theMeans.append(mean);
    edpLayout->addWidget(firstWidget);

    QLineEdit *secondLineEdit;
    QWidget *secondWidget ;
    if (valueType == 0)
        secondWidget = addLabeledLineEdit(QString("StdDev"), &secondLineEdit);
    else
     secondWidget = addLabeledLineEdit(QString("Max"), &secondLineEdit);
    secondLineEdit->setText(QString::number(stdDev));
    secondLineEdit->setDisabled(true);
    theStdDevs.append(stdDev);
    edpLayout->addWidget(secondWidget);

    edpLayout->addStretch();

    return edp;
}

void
ResultsGMT::addEarthquakeMotion(QString &name) {

    //
    // open event file, obtain json object, read into an earthquake record, obtain response spectrum
    //

    currentMethod = "LinearInterpolation";


    QFile file(name);
    if(file.open(QFile::ReadOnly)) {
        QString jsonText = QLatin1String(file.readAll());
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonText.toUtf8());
        QJsonObject jsonObj = jsonDoc.object();
        //EarthquakeRecord *theMotion = new EarthquakeRecord();
        //theMotion->inputFromJSON(jsonObj);
        // get dT, return error if not there
        QJsonValue theEvent = jsonObj["Events"];
        if (theEvent.isNull() || theEvent.isUndefined()) {
            return;
        }
        QJsonArray eventsArray = theEvent.toArray();
        foreach (const QJsonValue &eventValue, eventsArray) {
            QJsonObject eventObj = eventValue.toObject();
            QJsonValue theValue = eventObj["dT"];
            if (theValue.isNull() || theValue.isUndefined()) {
                return;
            }
            double dT=theValue.toDouble();
            theValue = eventObj["numSteps"];
            if (theValue.isNull() || theValue.isUndefined()) {
                return;
            }
            int numSteps =theValue.toInt();
            qDebug() << numSteps << " " << dT;

            theValue = eventObj["pattern"];
            if (theValue.isNull() || theValue.isUndefined()) {
                qDebug() << QString("ERROR: addEarthquakeMotion - no pattern");
                return;
            }
            QJsonArray patternsArray = theValue.toArray();
            foreach (const QJsonValue &pattern, patternsArray) {
                const QJsonObject patternObj = pattern.toObject();
                theValue = patternObj["dof"];
                if (theValue.isNull() || theValue.isUndefined()) {
                    return;
                }
                int dof =theValue.toInt();
                theValue = patternObj["timeSeries"];
                if (theValue.isNull() || theValue.isUndefined()) {
                    return;
                }
                QString patternTimeSeriesName =theValue.toString();
                qDebug() << QString("dof timeSeries: ") << dof << " " << patternTimeSeriesName;

                theValue = eventObj["timeSeries"];
                if (theValue.isNull() || theValue.isUndefined()) {
                    qDebug() << QString("ERROR: addEarthquakeMotion - no timeSeries");
                    return;
                }
                QJsonArray timeSeriesArray = theValue.toArray();
                foreach (const QJsonValue &timeSeriesValue, timeSeriesArray) {
                    QJsonObject timeSeriesObj = timeSeriesValue.toObject();
                    theValue = timeSeriesObj["name"];
                    if (theValue.isNull() || theValue.isUndefined()) {
                        qDebug() << QString("ERROR: addEarthquakeMotion - no timeSeries name");
                        return;
                    }
                    QString timeSeriesName =theValue.toString();
                    if (timeSeriesName == patternTimeSeriesName) {
                        qDebug() << "FOUND IT";
                        theValue = timeSeriesObj["data"];
                        if (theValue.isNull() || theValue.isUndefined()) {
                            qDebug() << QString("ERROR: addEarthquakeMotion - no data array");
                            return;
                        }
                        std::vector<double> data(numSteps);
                        QJsonArray dataArray = theValue.toArray();
                        for (int i=0; i<numSteps; i++)
                            data.push_back(dataArray.at(i).toDouble());

                        QVector<double> periods;
                        periods.append(0.1);
                        periods.append(0.5);
                        periods.append(1.0);
                        periods.append(2.0);
                        const char *integrator="LinearInterpolation";
                        double dT = 0.01;
                        double dampRatio = 0.0;
                        QVector<double> dispResponse;
                        QVector<double> accelResponse;

                        CalcResponseSpectrum(periods,
                                             dampRatio,
                                             integrator,
                                             data,
                                             dT,
                                             dispResponse,
                                             accelResponse);

                        qDebug() << "RESULT: " << dispResponse;
                        theGraphic->addData(dispResponse, periods);
                        //for (auto val : dispResponse) {qDebug() << "ONE " << val;}

                        break;
                    } else
                        qDebug() << timeSeriesName << " " << patternTimeSeriesName;
                }
            }

            qDebug() << numSteps << " " << dT;
        }

    }
}
