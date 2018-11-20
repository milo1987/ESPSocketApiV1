#include "Arduino.h"
#include "EspSocketApi.h"



EspSocketApi::EspSocketApi (String sname, String sversion) {
	_soft_name = sname;
	_soft_version = sversion;
	_api_version = "0.0.12";
}
EspSocketApi::EspSocketApi (String sname, String sversion, int pingintervall) {
	_pingintervall = pingintervall;
	EspSocketApi(sname, sversion);
}

void EspSocketApi::setWifi(const char* ssid, const char* pwd) {
	wiFiMulti.addAP(ssid, pwd);
}

void EspSocketApi::init(boolean setGroupfunction) {
	
	Serial.begin(115200);
	SPIFFS.begin();
	loadVars();
	
	if (setGroupfunction) {
		
		_groupfunction = true;
		
		loadVars();
		
		String grpname  = getSavedVar("grpsetting_name");
		String grpgroup = getSavedVar("grpsetting_gruppe");
		
		log (grpname);
		log (grpgroup);
		
		if (grpname != "n/a" && grpgroup != "n/a") {
			_soft_name = grpgroup + "." + grpname;
			log ("Gruppennamen konfiguriert. Setze gespeicherte Variabeln...");
		} else {
		 int i = _soft_name.indexOf(".");
		 addSavedVars("grpsetting_gruppe", _soft_name.substring(0,i));
		 addSavedVars("grpsetting_name", _soft_name.substring(i+1));
		 log ("Gruppenname noch nicht konfiguriert. Setze Variabeln....");
		}
		
	}
	
	
	WiFi.mode(WIFI_STA);
	WiFi.hostname(_soft_name.c_str());
	
}

void EspSocketApi::init() {
	init(false);	
}


void EspSocketApi::setSocketIO (String url, int port, std::function<void ()> connect_func,std::function<void (String var, String msg)> command_func) {
	_socketurl = url;
	_socketport = port;
	_clientCommandFunction = command_func;
	_clientConnectFunction = connect_func;
}

void EspSocketApi::startSocketIO() {

	webSocket.on("connect", [&](const char * payload, size_t length) { socketConnect(payload, length); });
	webSocket.on("disconnected", [&](const char * payload, size_t length) { socketDisconnected(payload, length); });
	webSocket.on("timeSync", [&](const char * payload, size_t length) { socketTimeSync(payload, length); });
	webSocket.on("ping", [&](const char * payload, size_t length) { socketPing(payload, length); });
	webSocket.on("command", [&](const char * payload, size_t length) {
		
		
		String msgString = String(payload); // Konvertierung der nachricht in ein String
		log ("Erhalte Command strg: " + msgString);
		int i = msgString.indexOf("~"); 
				  
		String head = msgString.substring(0, i);
		String msg =  msgString.substring(i+1);
		_clientCommandFunction(head, msg);
			
	});
	webSocket.on("webUpdate", [&](const char * payload, size_t length) { perform_web_update(payload, length); });
	webSocket.on("debug", [&](const char * payload, size_t length) { setDebug(String(payload)); });
	webSocket.on("reset", [&](const char * payload, size_t length) { reset(); });
	webSocket.on("setGrpname", [&](const char * payload, size_t length) { setGrpname(String(payload)); });
	
    webSocket.begin(_socketurl.c_str(), _socketport);
	

}

void EspSocketApi::setGrpname(String s) {
	
	int i = s.indexOf(":");
	addSavedVars("grpsetting_gruppe", s.substring(0,i));
	addSavedVars("grpsetting_name", s.substring(i+1));
	log ("Adaptergruppenname geändert. Resette....");
	delay(1000);
	reset();
	
}

void EspSocketApi::reset() {
	ESP.restart();
}


void EspSocketApi::socketConnect (const char * payload, size_t length) {
	
	log("Beginne mit SocketConnect", 0);
	
	
	webSocket.emit("init", String( "\"" + _soft_name + "!*!" + _soft_version + "!*!" + WiFi.localIP().toString() + "!*!" + _api_version + "!*!" + String(_groupfunction) + "!*!" + getSavedVar("sys-restartcounter") + "\"").c_str());
	
	
	_clientConnectFunction();
	log ("Ende von SocketConnect");
	
	
}

void EspSocketApi::socketDisconnected (const char * payload, size_t length) {
	log ("Socket disconnected");
}

void EspSocketApi::setDebug (String s) {
	log ("Set Debug Mode to: " + s);
	if (s == "true") { 
		_debug = true; 
	} else {
		_debug = false;
	}
}



void EspSocketApi::socketTimeSync (const char * payload, size_t length) {
	
	
	
	std::string str (payload);
	String s = String(payload);
	log ("TimeSync: " + s);
	int pos = str.find("#");
	
	String timestamp = s.substring(0,pos);
	String offset = s.substring(pos+1);
	
	log ("TStamp: " + timestamp);
	log ("Offset: " + String(offset.toInt()*-60));
		

	setTime(timestamp.toInt());
	adjustTime(offset.toInt()*-60);
	
	log ("Uhrzeit gesetzt: " + String( hour() ) + ":" + String( minute() ) ) ;
	
	
}

void EspSocketApi::socketPing (const char * payload, size_t length) {
	_lastping = millis();
}


// Sendet Variabeln Änderungen
void EspSocketApi::sendMsg(String varname, String msg) {
	webSocket.emit("command", ("\"" + varname + "~" + msg + "\"").c_str());
}
//Sendet Initialisierungbefehl mit Anlegung in Objekten
void EspSocketApi::initVars(String var, String wert) {
	webSocket.emit("initVars", String("\"" + var + "!*!" + wert + "\"").c_str() );
}



void EspSocketApi::loop() {
	
	
	
// Prüfe ob WLAN Verbunden	
	if (wiFiMulti.run() != WL_CONNECTED) {

		
		log ("WiFi nicht verbunden", 0);
		wifiinit = false;		// Setzt die Init beim "Neuverbinden"
		
	} else {
	// Wenn WLAN das erste mal verbunden:	
		if (wifiinit) {
			
			webSocket.loop();
			
			
			
		} else { // Ansonsten hier die Dauerschleife
			
			if (_socketport > 0) {
				startSocketIO();
				
			} else
				log ("Es wurde kein SocketIO Port gesetzt. Funktion setSocketIO nutzen!!");
			
			wifiinit = true;
		}
		
		
		
	}
		
}

boolean EspSocketApi::isConnected() {
	return _connection;
}

int EspSocketApi::loop (std::function<void ()> f) {

	while (_loop_timer <= (_loop_atimer + 1000)) {



        loop();
		f();

        delay(1);
        _loop_timer = millis();
  
   }

    _loop_atimer = _loop_timer;
	_loop_counter++;
	
	if (_loop_counter % _pingintervall == 0)
		webSocket.emit("ping","\"ping\"");

		//log ("Millis: " + String(millis()) + "; Lastping: " + String(_lastping+(_pingintervall*1000+5000)));
		if (_loop_counter % (_pingintervall+5) == 0) {
			if (millis() > _lastping+(_pingintervall*1000+5000) ) {
				
				
				if (_usePingTimeOut == 2) {
					_pingTimeOutFunction();
				} else if (_usePingTimeOut == 1) {
					
					log ("Ping Timeout läuft: " + String(_timeoutcounter));
					
					if (_timeoutcounter <= millis()+180000)
						timeOutAction();
				}
			
				
				_connection = false;
			} else {
				_connection = true;
				_timeoutcounter = millis();
			}
		}
	
	
    return _loop_counter;


}	


void EspSocketApi::setTimeOutAction(std::function<void ()> pingTimeOutFunction) {
	
	_usePingTimeOut = 2;
	_pingTimeOutFunction = pingTimeOutFunction;
	
}

void EspSocketApi::setTimeOutAction() {
	_usePingTimeOut = 1;
}

void EspSocketApi::timeOutAction() {
	
	int s = getSavedVar("sys-restartcounter").toInt() + 1;
	addSavedVars("sys-restartcounter", String(s));
	log ("Reboot, aufgrund von Timeout-Nr: " + String(s));
	delay(3000);
	reset();
}




void EspSocketApi::log(String txt, int loglvl) {
	
	// Loglevel:
		// 0 - Debug
		// 1 - Error
	
	Serial.println(txt);
	
	
	if (loglvl >= 1) {
		webSocket.emit("logerror", String("\"" + txt + "\"").c_str() );
	} else if (_debug) {
		webSocket.emit("logdebug", String("\"" + txt + "\"").c_str() );
	}

}

void EspSocketApi::log(String txt) { 
	log(txt, 0);
}





// Update Funktionen


void EspSocketApi::perform_web_update(const char * payload, size_t length) {
	
	webSocket.emit("webUpdate", String("\"***\"").c_str() );
	String fehlerString = "";
	String soft_link = String(payload);
	
	String name = _soft_name;
	
	if(_groupfunction) {
		int i = name.indexOf(".");
		name = name.substring(0,i);
	}

	HTTPClient httpClient;
	log ("Link: " + soft_link);
	httpClient.begin( soft_link );
	httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpCode = httpClient.POST("checkUpdate=true&name=" + name);
	
	
	String erg = httpClient.getString();
	
	log ("Update Code: " + String(httpCode));
	log ("Ergebnis: " + erg);
	
	
	if( httpCode == 200 ) {
	  
	  
	  float newVersion = erg.toFloat();
	  float oldVersion = _soft_version.toFloat();
	  
		  
		 log ( "Current firmware version: " + String(oldVersion) + "\n");
		 log ( "Available firmware version: " + String(newVersion) + "\n");
	  
	  
	  
		if (newVersion > oldVersion ) {
			
			httpClient.POST("checkFiles=true&name=" + name + "&version="+ erg);
			String checkFiles = httpClient.getString();
			if (checkFiles == "OK") {
			
				//String request = "?getSPIFFSFiles=true&name=" + _soft_name + "&version=" + erg;
				log( "Update wird geladen....\n");
							
							
						log ("Beginne mit OTA Update. \n");
						String request = "?getScetchFiles=true&name=" + name + "&version=" + erg;
						
						 t_httpUpdate_return ret = ESPhttpUpdate.update(soft_link + request);
						 boolean isSpiffsUpdate = false;
						 
						 if (ret == HTTP_UPDATE_OK) {
							 
							log ("Scetchupdate erfolgreich \n");
							 
							 
						 } else {
						 
							switch(ret) {
								case HTTP_UPDATE_FAILED:
									log("HTTP_UPDATE_FAILED Error: "  + ESPhttpUpdate.getLastErrorString() + "\n");
									break;

								case HTTP_UPDATE_NO_UPDATES:
									log("HTTP_UPDATE_NO_UPDATES \n");
									break;

								case HTTP_UPDATE_OK:
									log("HTTP_UPDATE_OK \n");
									break;
							}
									
						 }		
						

					
				
			} else {
				
				log ( "Eine &Umlt;berpr&uuml;fung beim Server ergab: " + checkFiles);
			}
			
			
		} else {
			
			log ( "Version ist aktuell. Kein Update erforderlich");
		}
	  
	  

	  
						
	} else {
		
		fehlerString = "Fehler beim Updateserver: " + httpClient.errorToString(httpCode);
	}	
	
	if (fehlerString != "")
		log (fehlerString, 1);
	
	
}





void EspSocketApi::addSavedVars(String name, String wert) {
	
	std::vector<String> str;
	str.push_back(name);
	str.push_back(wert);
	
	for(int x = 0; x < _saved_vars.size(); x++) {
		
		std::vector<String> str_alt = _saved_vars.at(x);
		
		
		if (str_alt.at(0) == name)  {			
			_saved_vars.erase(_saved_vars.begin() + x);
			log ("Variabel " + name + " vorhanden. Lösche!");
		} 
			
		
		
		
		
	}
	
	_saved_vars.push_back(str);
	
	
	saveVars();
	
	
	
	
	
}

String EspSocketApi::getSavedVar (String name) {
	
	boolean found = false;
	
	for(int x = 0; x < _saved_vars.size(); x++) {
		
		std::vector<String> str = _saved_vars.at(x);
		
		
		if (str.at(0) == name)  {
			return str.at(1);
			found = true;
		}
		
		
		
		
	}
	
	
	if (!found)
		return "n/a";
		
	
	
	
}


void EspSocketApi::saveVars() {
	
	File f = SPIFFS.open("/config", "w");
	
	for (int x = 0; x < _saved_vars.size(); x++) {
		
		std::vector<String> str = _saved_vars.at(x);
		f.print (str.at(0) + "~" + str.at(1) + "\n");
		

		
	}
	
	
	
	f.close();
	log ("Configdatei geschrieben");
	
	
	
}


void EspSocketApi::loadVars() {
	
	File f = SPIFFS.open("/config", "r");
	_saved_vars.clear();
	
	
	if (f) {
		
		while (f.available()){
			String s = f.readStringUntil('\n');
			String name = s.substring(0, s.indexOf("~"));
			String wert = s.substring(s.indexOf("~") + 1);
			
			
			log ("Name: " + name);
			log ("wert: " + wert);
			
			
			std::vector<String> vstr;
			vstr.push_back(name);
			vstr.push_back(wert);
			
		
			
			
			_saved_vars.push_back (vstr);
			
		}
	
	
		log ("Configdatei eingelesen");
		
		f.close();
		
		
		
	} else
		log ("Es existiert keine Configdatei");

	
	
}


// Hilfsfunktionen


String EspSocketApi::formatedTime() {
  String uhrzeit = String(hour()) + ":" + String(minute());
  if (minute() < 10) {    uhrzeit = String(hour()) + ":0" + String(minute());
  }
  
  return uhrzeit;
}




