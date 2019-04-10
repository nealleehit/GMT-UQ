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
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
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

#include "WorkflowAppGMT.h"

// Qt Widgets
#include <QPushButton>
#include <QScrollArea>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QLabel>
#include <QDebug>
#include <QHBoxLayout>
#include <QTreeView>
#include <QStandardItemModel>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QStackedWidget>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <RemoteService.h>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QHostInfo>
#include <QUuid>
#include <QSettings>
#include "CustomizedItemModel.h"

// SimCenter Widgets
#include <LocationInformation.h>
#include <EarthquakeEventSelection.h>
#include <ResultsGMT.h>
#include <RunLocalWidget.h>
#include <RemoteService.h>
#include <RandomVariablesContainer.h>
#include <InputWidgetSampling.h>

#include <LocalApplication.h>
#include <RemoteApplication.h>
#include <RemoteJobManager.h>
#include <RunWidget.h>
#include <InputWidgetUQ.h>

#include "CustomizedItemModel.h"
#include <GoogleAnalytics.h>

// static pointer for global procedure set in constructor
static WorkflowAppGMT *theApp = 0;

// global procedure
int getNumParallelTasks() {
    return theApp->getMaxNumParallelTasks();
}

WorkflowAppGMT::WorkflowAppGMT(RemoteService *theService, QWidget *parent)
    : WorkflowAppWidget(theService, parent)
{

    theApp = this;

    //
    // create the various widgets
    //

    theRVs = new RandomVariablesContainer();
    theLoc = new LocationInformation();
    theEvent = new EarthquakeEventSelection(theRVs);
    theUQ_Method = new InputWidgetSampling();
    theResults = new ResultsGMT();

    localApp = new LocalApplication("GMT.py");
    remoteApp = new RemoteApplication("GMT.py", theService);
    theJobManager = new RemoteJobManager(theService);

    // theRunLocalWidget = new RunLocalWidget(theUQ_Method);
    SimCenterWidget *theWidgets[1];
    //    theWidgets[0] = theAnalysis;
    //    theWidgets[1] = theUQ_Method;
    int numWidgets = 2;
    theRunWidget = new RunWidget(localApp, remoteApp, theWidgets, 0);

    //
    // connect signals and slots
    //

    // error messages and signals
    connect(theResults,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(theResults,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(theResults,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));

    connect(theLoc,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(theLoc,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(theLoc,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));

    connect(theEvent,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(theEvent,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(theEvent,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));

    connect(theRunWidget,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(theRunWidget,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(theRunWidget,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));

    connect(localApp,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(localApp,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(localApp,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));

    connect(remoteApp,SIGNAL(sendErrorMessage(QString)), this,SLOT(errorMessage(QString)));
    connect(remoteApp,SIGNAL(sendStatusMessage(QString)), this,SLOT(statusMessage(QString)));
    connect(remoteApp,SIGNAL(sendFatalMessage(QString)), this,SLOT(fatalMessage(QString)));


    connect(localApp,SIGNAL(setupForRun(QString &,QString &)), this, SLOT(setUpForApplicationRun(QString &,QString &)));
    connect(this,SIGNAL(setUpForApplicationRunDone(QString&, QString &)), theRunWidget, SLOT(setupForRunApplicationDone(QString&, QString &)));

    connect(localApp,
            SIGNAL(processResults(QString, QString, QString)),
            this,
            SLOT(processResults(QString, QString, QString)));

    connect(remoteApp,SIGNAL(setupForRun(QString &,QString &)), this, SLOT(setUpForApplicationRun(QString &,QString &)));

    connect(theJobManager,
            SIGNAL(processResults(QString , QString, QString)),
            this,
            SLOT(processResults(QString, QString, QString)));
    connect(theJobManager,SIGNAL(loadFile(QString)), this, SLOT(loadFile(QString)));

    connect(remoteApp,SIGNAL(successfullJobStart()), theRunWidget, SLOT(hide()));

    //connect(theRunLocalWidget, SIGNAL(runButtonPressed(QString, QString)), this, SLOT(runLocal(QString, QString)));

    //
    // some of above widgets are inside some tabbed widgets
    //

    theUQ = new InputWidgetUQ(theUQ_Method,theRVs);

    //
    //  NOTE: for displaying the widgets we will use a QTree View to label the widgets for selection
    //  and we will use a QStacked widget for displaying the widget. Which of widgets displayed in StackedView depends on
    //  item selected in tree view.
    //

    //
    // create layout to hold tree view and stackedwidget
    //

    horizontalLayout = new QHBoxLayout();
    this->setLayout(horizontalLayout);

    //
    // create a TreeView widget & provide items for each widget to be displayed & add to layout
    //

    treeView = new QTreeView();
    standardModel = new CustomizedItemModel;// QStandardItemModel ;
    QStandardItem *rootNode = standardModel->invisibleRootItem();

    //defining bunch of items for inclusion in model
    QStandardItem *giItem = new QStandardItem("LOC");
    QStandardItem *evtItem = new QStandardItem("EVT");
    QStandardItem *uqItem   = new QStandardItem("UQ");
    QStandardItem *resultsItem = new QStandardItem("RES");

    //building up the hierarchy of the model
    rootNode->appendRow(giItem);
    rootNode->appendRow(evtItem);
    rootNode->appendRow(uqItem);
    rootNode->appendRow(resultsItem);

    infoItemIdx = rootNode->index();

    //register the model
    treeView->setModel(standardModel);
    treeView->expandAll();
    treeView->setHeaderHidden(true);
    treeView->setMaximumWidth(100);
    treeView->setMinimumWidth(100);
    treeView->setEditTriggers(QTreeView::EditTrigger::NoEditTriggers);//Disable Edit for the TreeView

    //
    // customize the apperance of the menu on the left
    //

    treeView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff ); // hide the horizontal scroll bar
    treeView->setObjectName("treeViewOnTheLeft");
    treeView->setIndentation(0);
    QFile fileeeuq(":/styles/stylesheet_eeuq.qss");
    QFile filebar(":/styles/menuBar.qss");
    if(fileeeuq.open(QFile::ReadOnly) && filebar.open(QFile::ReadOnly)) {
        QString styleeeuq = QLatin1String(fileeeuq.readAll());
        QString stylebar = QLatin1String(filebar.readAll());
        this->setStyleSheet(styleeeuq + stylebar);
        fileeeuq.close();
        filebar.close();
    }
    else
        qDebug() << "Open Style File Failed!";



    //
    // set up so that a slection change triggers the selectionChanged slot
    //

    QItemSelectionModel *selectionModel= treeView->selectionModel();
    connect(selectionModel,
            SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            this,
            SLOT(selectionChangedSlot(const QItemSelection &, const QItemSelection &)));

    // add the TreeView widget to the layout
    horizontalLayout->addWidget(treeView);

    //
    // create the staked widget, and add to it the widgets to be displayed, and add the stacked widget itself to layout
    //

    theStackedWidget = new QStackedWidget();
    theStackedWidget->addWidget(theLoc);
    theStackedWidget->addWidget(theEvent);
    theStackedWidget->addWidget(theUQ);
    theStackedWidget->addWidget(theResults);

    // add stacked widget to layout
    horizontalLayout->addWidget(theStackedWidget);

    // set current selection to GI
    treeView->setCurrentIndex( infoItemIdx );

    // access a web page which will increment the usage count for this tool
    manager = new QNetworkAccessManager(this);

    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));

    manager->get(QNetworkRequest(QUrl("http://opensees.berkeley.edu/OpenSees/developer/eeuq/use.php")));

    // access a web page which will increment the usage count for this tool
    manager = new QNetworkAccessManager(this);

    connect(manager, SIGNAL(finished(QNetworkReply*)),
            this, SLOT(replyFinished(QNetworkReply*)));

    manager->get(QNetworkRequest(QUrl("http://opensees.berkeley.edu/OpenSees/developer/bfm/use.php")));
    //  manager->get(QNetworkRequest(QUrl("https://simcenter.designsafe-ci.org/multiple-degrees-freedom-analytics/")));


    /*
    QFile fileS(":/styles/stylesheet.qss");
    if(fileS.open(QFile::ReadOnly)) {
        treeView->setStyleSheet(fileS.readAll());
        fileS.close();
    }
    else
        qDebug() << "Open Style File Failed!";
     */ //It seems this has been done in previous lines.

}

WorkflowAppGMT::~WorkflowAppGMT()
{

}


void
WorkflowAppGMT::selectionChangedSlot(const QItemSelection & /*newSelection*/, const QItemSelection &/*oldSelection*/) {

    //get the text of the selected item
    const QModelIndex index = treeView->selectionModel()->currentIndex();
    QString selectedText = index.data(Qt::DisplayRole).toString();

    if (selectedText == "LOC")
        theStackedWidget->setCurrentIndex(0);
    else if (selectedText == "EVT")
        theStackedWidget->setCurrentIndex(1);
    else if (selectedText == "UQ")
        theStackedWidget->setCurrentIndex(2);
    else if (selectedText == "RES")
        theStackedWidget->setCurrentIndex(3);
}


bool
WorkflowAppGMT::outputToJSON(QJsonObject &jsonObjectTop) {
    //
    // get each of the main widgets to output themselves
    //

    QJsonObject apps;

    QJsonObject jsonObjGenInfo;
    theLoc->outputToJSON(jsonObjGenInfo);
    jsonObjectTop["GeneralInformation"] = jsonObjGenInfo;

    theRVs->outputToJSON(jsonObjectTop);

    QJsonObject jsonObjectUQ;
    theUQ_Method->outputToJSON(jsonObjectUQ);
    jsonObjectTop["UQ_Method"] = jsonObjectUQ;

    QJsonObject appsUQ;
    theUQ_Method->outputAppDataToJSON(appsUQ);
    apps["UQ"]=appsUQ;

   // NOTE: Events treated differently, due to array nature of objects
    theEvent->outputToJSON(jsonObjectTop);
    theEvent->outputAppDataToJSON(apps);

    theRunWidget->outputToJSON(jsonObjectTop);
    jsonObjectTop["Applications"]=apps;
    
    return true;
}


void
WorkflowAppGMT::processResults(QString dakotaOut, QString dakotaTab, QString inputFile) {


   theResults->processResults(dakotaOut, dakotaTab, inputFile);
   theRunWidget->hide();
   treeView->setCurrentIndex(infoItemIdx);
   theStackedWidget->setCurrentIndex(3);
 }

void
WorkflowAppGMT::clear(void)
{
    theLoc->clear();
}

bool
WorkflowAppGMT::inputFromJSON(QJsonObject &jsonObject)
{
    //
    // get each of the main widgets to input themselves
    //


    if (jsonObject.contains("GeneralInformation")) {
        QJsonObject jsonObjGeneralInformation = jsonObject["GeneralInformation"].toObject();
        theLoc->inputFromJSON(jsonObjGeneralInformation);
    } else
        return false;


    /*
    ** Note to me - RVs and Events treated differently as both use arrays .. rethink API!
    */

    if (jsonObject.contains("UQ_Method")) {
        QJsonObject jsonObjUQInformation = jsonObject["UQ"].toObject();
        theEvent->inputFromJSON(jsonObjUQInformation);
    } else
        return false;

    if (jsonObject.contains("Applications")) {

        QJsonObject theApplicationObject = jsonObject["Applications"].toObject();

        // note: Events is different because the object is an Array
        if (theApplicationObject.contains("Events")) {
            QJsonObject theObject = theApplicationObject["Events"].toObject();
            theEvent->inputAppDataFromJSON(theApplicationObject);
        } else {
            return false;     
        }


        if (theApplicationObject.contains("UQ")) {
            QJsonObject theObject = theApplicationObject["UQ"].toObject();
            theUQ_Method->inputAppDataFromJSON(theObject);
        } else {
            return false;
        }

    } else
        return false;

    theEvent->inputFromJSON(jsonObject);
    theRVs->inputFromJSON(jsonObject);
    theRunWidget->inputFromJSON(jsonObject);


    return true;
}


void
WorkflowAppGMT::onRunButtonClicked() {
    theRunWidget->showLocalApplication();
    GoogleAnalytics::ReportLocalRun();
}

void
WorkflowAppGMT::onRemoteRunButtonClicked(){
    emit errorMessage("");

    bool loggedIn = theRemoteService->isLoggedIn();

    if (loggedIn == true) {

        theRunWidget->hide();
        theRunWidget->showRemoteApplication();

    } else {
        errorMessage("ERROR - You Need to Login");
    }

    GoogleAnalytics::ReportDesignSafeRun();
}

void
WorkflowAppGMT::onRemoteGetButtonClicked(){

    emit errorMessage("");

    bool loggedIn = theRemoteService->isLoggedIn();

    if (loggedIn == true) {

        theJobManager->hide();
        theJobManager->updateJobTable("");
        theJobManager->show();

    } else {
        errorMessage("ERROR - You Need to Login");
    }
}

void
WorkflowAppGMT::onExitButtonClicked(){

}

void
WorkflowAppGMT::setUpForApplicationRun(QString &workingDir, QString &subDir) {

    errorMessage("");

    //
    // create temporary directory in working dir
    // and copy all files needed to this directory by invoking copyFiles() on app widgets
    //

    QString tmpDirName = QString("tmp.SimCenter");
    qDebug() << "TMP_DIR: " << tmpDirName;
    QDir workDir(workingDir);

    QString tmpDirectory = workDir.absoluteFilePath(tmpDirName);
    QDir destinationDirectory(tmpDirectory);

    if(destinationDirectory.exists()) {
      destinationDirectory.removeRecursively();
    } else
      destinationDirectory.mkpath(tmpDirectory);

    QString templateDirectory  = destinationDirectory.absoluteFilePath(subDir);
    destinationDirectory.mkpath(templateDirectory);

    qDebug() << "templateDir: " << templateDirectory;

    theEvent->copyFiles(templateDirectory);
    theUQ_Method->copyFiles(templateDirectory);

    //
    // in new templatedir dir save the UI data into dakota.json file (same result as using saveAs)
    // NOTE: we append object workingDir to this which points to template dir
    //

    QString inputFile = templateDirectory + QDir::separator() + tr("dakota.json");

    QFile file(inputFile);
    if (!file.open(QFile::WriteOnly | QFile::Text)) {
        //errorMessage();
        return;
    }
    QJsonObject json;
    this->outputToJSON(json);

    json["runDir"]=tmpDirectory;
    json["WorkflowType"]="Building Simulation";


    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();


    statusMessage("SetUp Done .. Now starting application");

    emit setUpForApplicationRunDone(tmpDirectory, inputFile);
}

void
WorkflowAppGMT::loadFile(const QString fileName){

    //
    // open file
    //

    QFile file(fileName);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        emit errorMessage(QString("Could Not Open File: ") + fileName);
        return;
    }

    //
    // place contents of file into json object
    //

    QString val;
    val=file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(val.toUtf8());
    QJsonObject jsonObj = doc.object();

    // close file
    file.close();

    //
    // clear current and input from new JSON
    //

    this->clear();
    this->inputFromJSON(jsonObj);

}


int
WorkflowAppGMT::getMaxNumParallelTasks() {
    return theUQ_Method->getNumParallelTasks();
}