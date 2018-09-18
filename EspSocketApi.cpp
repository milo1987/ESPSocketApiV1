#include "Arduino.h"
#include "EspSocketApi.h"



EspSocketApi::EspSocketApi (String sname, String sversion) {
	_soft_name = sname;
	_soft_version = sversion;
	_api_version = "0.0.6";
}

void EspSocketApi::setWifi(const char* ssid, const char* pwd) {
	wiFiMulti.addAP(ssid, pwd);
}

void EspSocketApi::init() {
	
	Serial.begin(115200);
	SPIFFS.begin();
	WiFi.mode(WIFI_STA);
	WiFi.hostname(_soft_name.c_str());
	
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
	webSocket.on("command", [&](const char * payload, size_t length) {
		
		String msgString = String(payload); // Konvertierung der nachricht in ein String
		int i = msgString.indexOf("~"); 
				  
		String head = msgString.substring(0, i);
		String msg =  msgString.substring(i+1);
		_clientCommandFunction(head, msg);
			
	});
	webSocket.on("webUpdate", [&](const char * payload, size_t length) { perform_web_update(payload, length); });
	webSocket.on("debug", [&](const char * payload, size_t length) { setDebug(String(payload)); });
	
    webSocket.begin(_socketurl.c_str(), _socketport);
	

}


void EspSocketApi::socketConnect (const char * payload, size_t length) {
	
	log("Beginne mit SocketConnect", 0);
	
	webSocket.emit("init", String( "\"" + _soft_name + "!*!" + _soft_version + "!*!" + WiFi.localIP().toString() + "!*!" + _api_version +  "\"").c_str());
	
	
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
				log ("Es wurde kein SocketIO Port gesetzt. Funktion setSocketIO!!");
			
			wifiinit = true;
		}
		
		
		
	}
		
}

int EspSocketApi::loop1sek (std::function<void ()> f) {

	while (_loop_timer <= (_loop_atimer + 1000)) {



        loop();
		f();

        delay(1);
        _loop_timer = millis();
  
   }

    _loop_atimer = _loop_timer;
    return ++_loop_counter;


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
	
	String fehlerString = "";
	String soft_link = String(payload);

	HTTPClient httpClient;
	log ("Link: " + soft_link);
	httpClient.begin( soft_link );
	httpClient.addHeader("Content-Type", "application/x-www-form-urlencoded");
	int httpCode = httpClient.POST("checkUpdate=true&name=" + _soft_name);
	
	
	String erg = httpClient.getString();
	
	log ("Update Code: " + String(httpCode));
	log ("Ergebnis: " + erg);
	
	
	if( httpCode == 200 ) {
	  
	  
	  float newVersion = erg.toFloat();
	  float oldVersion = _soft_version.toFloat();
	  
		  
		 fehlerString += "Current firmware version: " + String(oldVersion) + "\n";
		 fehlerString += "Available firmware version: " + String(newVersion) + "\n";
	  
	  
	  
		if (newVersion > oldVersion ) {
			
			httpClient.POST("checkFiles=true&name=" + _soft_name + "&version="+ erg);
			String checkFiles = httpClient.getString();
			if (checkFiles == "OK") {
			
				//String request = "?getSPIFFSFiles=true&name=" + _soft_name + "&version=" + erg;
				fehlerString += "Update wird geladen....\n";
				
				//log ("Beginne mit SPIFFS Aktualisierung",0);
						

						
				/* SPIFFS UPDATE gelöscht, da nicht mehr benötigt!
				
				
				t_httpUpdate_return ret = ESPhttpUpdate.updateSpiffs(soft_link + request);
				 boolean isSpiffsUpdate = false;
				 
				 if (ret == HTTP_UPDATE_OK) {
					 
					 log ("SpiffsUpdate erfolgreich",0);
					 isSpiffsUpdate = true;
					 
				 } else {
				 
					switch(ret) {
						case HTTP_UPDATE_FAILED:
							log("HTTP_UPDATE_FAILED Error: "  + ESPhttpUpdate.getLastErrorString(),0);
							fehlerString = "HTTP_UPDATE_FAILED Error: "  + ESPhttpUpdate.getLastErrorString();
							break;

						case HTTP_UPDATE_NO_UPDATES:
							log("HTTP_UPDATE_NO_UPDATES",0);
							break;

						case HTTP_UPDATE_OK:
							log("HTTP_UPDATE_OK",0);
							break;
					}
							
				 }		
				 
				 
					*/		
		
						
						
					boolean isSpiffsUpdate = true;
					
					
					if (isSpiffsUpdate) {
							
						//log ("Schreibe config neu.", 0);
						//saveConfigData();
							
							
						fehlerString += "Beginne mit OTA Update. \n";
						String request = "?getScetchFiles=true&name=" + _soft_name + "&version=" + erg;
						
						t_httpUpdate_return ret = ESPhttpUpdate.update(soft_link + request);
						 boolean isSpiffsUpdate = false;
						 
						 if (ret == HTTP_UPDATE_OK) {
							 
							fehlerString += "Scetchupdate erfolgreich \n";
							 
							 
						 } else {
						 
							switch(ret) {
								case HTTP_UPDATE_FAILED:
									fehlerString +=("HTTP_UPDATE_FAILED Error: "  + ESPhttpUpdate.getLastErrorString() + "\n",0);
									break;

								case HTTP_UPDATE_NO_UPDATES:
									fehlerString +=("HTTP_UPDATE_NO_UPDATES \n",0);
									break;

								case HTTP_UPDATE_OK:
									fehlerString +=("HTTP_UPDATE_OK \n",0);
									break;
							}
									
						 }		
						

					} else
						fehlerString += ("Abbruch des Updates, SPIFFS failed.",0);
				
			} else {
				
				fehlerString += "Eine &Umlt;berpr&uuml;fung beim Server ergab: " + checkFiles;
			}
			
			
		} else {
			
			fehlerString += "Version ist aktuell. Kein Update erforderlich";
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




