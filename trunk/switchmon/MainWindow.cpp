#include "MainWindow.h"

#include "VideoWidget.h"
#include "VideoReceiver.h"

#include "../3rdparty/qjson/parser.h"

MainWindow::MainWindow()
	: QWidget()
	, m_ua(0)
	, m_lastReqType(T_UNKNOWN)
	, m_isConnected(false)
	, m_parser(new QJson::Parser)
{
	QSettings settings;
	
	// Setup the layout of the window
	QVBoxLayout *vbox = new QVBoxLayout(this);
	QHBoxLayout *conLay = new QHBoxLayout();
	conLay->addWidget(new QLabel("Server: "));
	
	m_serverBox = new QLineEdit();
	m_serverBox->setText(settings.value("lastServer","10.10.9.90").toString());
	connect(m_serverBox, SIGNAL(textChanged(QString)), this, SLOT(textChanged(QString)));
	conLay->addWidget(m_serverBox);
	
	m_connectBtn = new QPushButton("Connect");
	connect(m_connectBtn, SIGNAL(clicked()), this, SLOT(connectToServer()));
	conLay->addWidget(m_connectBtn);
	
	conLay->addStretch(1);
	
	vbox->addLayout(conLay);
	
	m_hbox = new QHBoxLayout();
	m_hbox->setContentsMargins(0,0,0,0);
	
	vbox->addLayout(m_hbox);
	
	setWindowTitle("Camera Monitor");
	
	
	connectToServer();
	
// 	// Finally, get down to actually creating the drawables 
// 	// and setting their positions
// 	for(int i=0; i<numItems; i++)
// 	{
// 		// Load the connection string
// 		QString con = settings.value(tr("input%1").arg(i)).toString();
// 		//if(con.indexOf("=") < 0)
// 		//	con = tr("net=%1").arg(con);
// 			
// 		//qDebug() << "Input "<<i<<": "<<con;
// 		QStringList parts = con.split(":");
// 		QString host = parts[0];
// 		int port = parts[1].toInt();
// 		qDebug() << "Input "<<i<<": host:"<<host<<", port:"<<port;
// 		
// 		VideoWidget *widget = new VideoWidget();
// 		VideoReceiver *receiver = VideoReceiver::getReceiver(host,port);
// 		
// 		widget->setVideoSource(receiver);
// 		widget->setOverlayText(tr("Cam %1").arg(i+1));
// 		widget->setProperty("num", i);
// 		
// 		connect(widget, SIGNAL(clicked()), this, SLOT(vidWidgetClicked()));
// 		
// 		hbox->addWidget(widget);
// 	}
}

MainWindow::~MainWindow()
{
	delete m_parser;	
}

void MainWindow::connectToServer()
{
	m_isConnected = false;
	m_host = m_serverBox->text();
	
	m_lastReqType = T_EnumInputs;
	loadUrl(tr("http://%1:9979/ListVideoInputs").arg(m_host));
	
	m_connectBtn->setEnabled(false);
	m_connectBtn->setText("Connecting...");
	
	QSettings().setValue("lastServer",m_host);
}

void MainWindow::textChanged(const QString&)
{
	m_connectBtn->setEnabled(true);
	m_connectBtn->setText("Connect");
}

void MainWindow::vidWidgetClicked()
{
	QObject *obj = sender();
	//qDebug() << "vidWidgetClicked: clicked num: "<<obj->property("num").toInt();
	QString con = obj->property("con").toString();
	QString escapedCon = con; //QString::fromUtf8(con.toUtf8().toPercentEncoding());
	qDebug() << "vidWidgetClicked: clicked connection: "<<con;
	
	m_lastReqType = T_SetProperty;
	//http://localhost:9979/SetUserProperty?drawableid=1293&name=videoInput&value=/dev/video1&type=string
	loadUrl(tr("http://%1:9979/SetUserProperty?drawableid=%2&name=videoConnection&value=%3&type=string").arg(m_host).arg(m_drawableId).arg(escapedCon));
}

// Why? Just for easier setting the win-width/-height in the .ini file.
void MainWindow::resizeEvent(QResizeEvent*)
{
	//qDebug() << "Window Size: "<<width()<<" x "<<height(); 
}


void MainWindow::loadUrl(const QString &location) 
{
	QUrl url(location);
	
	//qDebug() << "MjpegClient::loadUrl(): url:"<<url;

	if(!m_ua)
	{
		m_ua = new QNetworkAccessManager(this);
		connect(m_ua, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleNetworkData(QNetworkReply*)));
	}
	
	m_ua->get(QNetworkRequest(url));
}

void MainWindow::handleNetworkData(QNetworkReply *networkReply) 
{
	QUrl url = networkReply->url();
	if (!networkReply->error())
	{
		QByteArray bytes = networkReply->readAll();
		//m_isConnected = true;
		switch(m_lastReqType)
		{
			case T_EnumInputs:
				processInputEnumReply(bytes);
				break;
			case T_ExamineScene:
				processExamineSceneReply(bytes);
				break;
			case T_SetProperty:
				// assume successful
				break;
			default:
				// nothing right now
				break;
		};
		
	}
	
	networkReply->deleteLater();
	//networkReply->manager()->deleteLater();
}

void MainWindow::processInputEnumReply(const QByteArray &bytes)
{
	// Clear the slider grid of old controls
	while(m_hbox->count() > 0)
	{
		QLayoutItem *item = m_hbox->itemAt(m_hbox->count() - 1);
		m_hbox->removeItem(item);
		if(QWidget *widget = item->widget())
		{
			// disconnect any slots
			disconnect(widget, 0, this, 0);
			
			m_hbox->removeWidget(widget);
			delete widget;
		}
			
		delete item;
		item = 0;
	}
	
	
	bool ok = false;
	QVariant reply = m_parser->parse(bytes,&ok);
	// Sample: { "cmd" : "ListVideoInputs", "list" : [ "dev=/dev/video0,input=Composite1,net=10.10.9.90:7755", "dev=/dev/video1,input=Composite1,net=10.10.9.90:7756" ] }
	QVariantMap replyMap = reply.toMap();
	QVariantList inputList = replyMap["list"].toList();
	int idx =0;
	foreach(QVariant entry, inputList)
	{
		QString con = entry.toString();
		
		QHash<QString,QString> map;
		QStringList opts = con.split(",");
		foreach(QString pair, opts)
		{
			QStringList values = pair.split("=");
			if(values.size() < 2)
			{
				qDebug() << "MainWindow::processInputEnumReply: Parse error for option:"<<pair;
				continue;
			}
	
			QString name = values[0].toLower();
			QString value = values[1];
	
			map[name] = value;
		}
		
		QStringList parts = map["net"].split(":");
		QString host = parts[0];
		int port = parts[1].toInt();
		qDebug() << "Input "<<idx<<": host:"<<host<<", port:"<<port;
		
		VideoWidget *widget = new VideoWidget();
		VideoReceiver *receiver = VideoReceiver::getReceiver(host,port);
		
		widget->setVideoSource(receiver);
		widget->setOverlayText(tr("Cam %1").arg(idx+1));
		widget->setProperty("con", con);
		
		connect(widget, SIGNAL(clicked()), this, SLOT(vidWidgetClicked()));
		
		m_hbox->addWidget(widget);
		
		idx++;
	}
	
	adjustSize();
	
	m_lastReqType = T_ExamineScene;
	loadUrl(tr("http://%1:9979/ExamineCurrentScene").arg(m_host));
	
	m_connectBtn->setEnabled(false);
	m_connectBtn->setText("Checking...");
}

void MainWindow::processExamineSceneReply(const QByteArray &bytes)
{
	bool ok = false;
	QVariant reply = m_parser->parse(bytes,&ok);
	// Sample: { "cmd" : "ListVideoInputs", "list" : [ "dev=/dev/video0,input=Composite1,net=10.10.9.90:7755", "dev=/dev/video1,input=Composite1,net=10.10.9.90:7756" ] }
	QVariantMap replyMap = reply.toMap();
// 	- GET http://localhost:9979/ExamineCurrentScene
// 		- RESPONSE:
// 			{ "cmd" : "ExamineCurrentScene", 
// 				"items" : 
// 				[ 
// 					{ 
// 						"id" : 1293, "name" : "", "playlist" : [  ], 
// 						"props" : 
// 						[ 
// 							{ "name" : "objectName", "type" : "QString", "value" : "" }
// 							, { "name" : "id", "type" : "int", "value" : "1293" }
// 							...
// 							, { "name" : "videoConnection", "type" : "QString", "value" : "dev=/dev/video0,input=Composite1,net=10.10.9.90:7755" }
// 							, { "name" : "videoInput", "type" : "QString", "value" : "/dev/video0" }
// 						]
// 						, "type" : "VideoInput" 
// 					}
// 				]
// 				, "status" : "true" 
// 			}
// 			
// 			var item = obj.items[0];
// 			var vidItemID = item.id if item.type == "VideoInput";
// 			
	QVariantList itemList = replyMap["items"].toList();
	
	m_drawableId = -1;
	foreach(QVariant itemData, itemList)
	{
		if(m_drawableId < 0)
		{
			QVariantMap itemMap = itemData.toMap();
			if(itemMap["type"].toString() == "VideoInput")
				m_drawableId = itemMap["id"].toInt();
		}
	}
		
	m_connectBtn->setEnabled(false);
	m_connectBtn->setText("Connected");
}
