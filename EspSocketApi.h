#ifndef EspSocketApi_h
#define EspSocketApi_h

// Arduino
#include "Arduino.h"

// Serielle Schnittstelle & Spiffs
#include "SPI.h"  
#include "FS.h"

// ESP8266 Lib
#include <ESP8266httpUpdate.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>

#include <SocketIoClient.h>

#include <TimeLib.h>
#include <string>
#include <vector>


class EspSocketApi {
	
	public:
		EspSocketApi(String sname, String sversion);
		EspSocketApi(String sname, String sversion, int pingintervall);
		void setWifi(const char* ssid, const char* pwd);
		void init();
		void setSocketIO (	String url, 
							int port, 
							std::function<void ()> connect_func,
							std::function<void (String var, String msg)> command_func);

		void log(String txt, int loglvl);
		void log(String txt);
		
		
		
		int loop (std::function<void ()> f);
		void sendMsg(String varname, String msg);
		void initVars(String var, String wert);
		
		void addSavedVars(String name, String wert);
		String getSavedVar (String name);
		void loadVars();
		void saveVars();
		
		// Hilfsfunktionen
		
		String formatedTime();
		
	
	private:
	
		String _soft_name = "";					// Softwarename
		String _soft_version = "";				// Software Version
		String _soft_link = "";
		String _api_version = "";
		boolean _debug = false;					// Debugmodus, sendet alles unter log an den Server
	
		ESP8266WiFiMulti wiFiMulti;				// Wifi
		SocketIoClient webSocket;				// Socketverbindung
		
		
		// Socket IO Variabeln
		String _socketurl = "";
		int _socketport = 0;
		std::function<void ()> _clientConnectFunction;
		std::function<void (String var, String msg)> _clientCommandFunction;
		
		void loop();
		
		// Socket IO Verbindungsfunktionen		
		void socketConnect(const char * payload, size_t length);	// Bei Verbindung wird diese F aufgerufen
		void socketTimeSync(const char * payload, size_t length);
		void socketDisconnected (const char * payload, size_t length);
		void perform_web_update (const char * payload, size_t length);
		void socketPing (const char * payload, size_t length);
		void setDebug (String s);
		void startSocketIO();		// Startet die SocketIO Verbindung
		
		// Speicherfunktionen für Zustände
		std::vector<std::vector<String>> _saved_vars;
		
		
		// Variabeln für die exakte Loopfunktion
		int _loop_timer = 0;
		int _loop_atimer = -1;
		int _loop_counter = 0;
		int _pingintervall = 10;
	
	
		// Timestamp
		int timestamp = 0;
		
	
		// Wifisteuerung
		boolean wifiinit = false;
	
		// Hilfsfunktionen
		
	
	
};




#endif