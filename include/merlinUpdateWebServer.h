#pragma once

#include <ArduinoOTA.h>

#ifdef ESP8266
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#endif

#ifdef ESP32
#include <WebServer.h>
#endif



/* Style */
String style =
"<style>#file-input,input{width:40%;height:40px;border-radius:4px;margin:5px auto;font-size:15px}"
"input{background:#f1f1f1;border:0;padding:0 5px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"select{background:#f1f1f1;border:0;padding:0 5px}body{background:#3498db;font-family:sans-serif;font-size:14px;color:#777}"
"#file-input{padding:0;border:1px solid #ddd;line-height:44px;text-align:center;display:block;cursor:pointer}"
"#bar,#prgbar{background-color:#f1f1f1;border-radius:10px}#bar{background-color:#3498db;width:0%;height:10px}"
"div{background:#fff;max-width:600px;margin:5px auto;padding:5px;border-radius:5px;text-align:left;font-family:sans-serif;font-size:14px;}"
"form{background:#fff;max-width:600px;margin:5px auto;padding:5px;border-radius:5px;text-align:center;font-family:sans-serif;font-size:14px;}"
".btn{background:#3498db;color:#fff;cursor:pointer}"
"svg[data-lastpass-icon=true]{visibility:none;display: none !important;}"
"div[data-lastpass-icon-root=true]{visibility:none;display: none !important;}"
"div[data-lastpass-icon-root]{visibility:none;display: none !important;}"
"div[data-lastpass-root]{visibility:none;display: none !important;}"
"div[data-lastpass-infield]{visibility:none;display: none !important;}"
"div[data-lastpass-infield='true']{visibility:none;display: none !important;}"
"hr.new5 { border: 1px solid #3498db;  }"



"</style>";

/* Login page */
String loginIndex =
"<form name=loginForm>"
"<input name=userid data-lpignore='true' placeholder='User ID' width:100%> "
"<input name=pwd data-lpignore='true' placeholder=Password type=Password> "
"<input type=submit onclick=check(this.form) class=btn value='Firmware Update - Login'>"
"<br>";

String loginIndex2 =
"</form>"
"<script>"
"const editor ="
"document.querySelector('grammarly-editor-plugin');"
"editor.config = {"
"suggestionCards: 'off'"
"};"
"function check(form) {"
"if(form.userid.value=='admin' && form.pwd.value=='1llusion_mb')"
"{window.open('/serverIndex')}"
"else"
"{alert('Error Password or Username')}"
"}"
"</script>"  ;

/* Server Index Page */
String serverIndex =
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
"<input type='file' name='update' id='file' onchange='sub(this)' style=display:none>"
"<label id='file-input' for='file'>   Choose file...</label>"
"<input type='submit' class=btn value='Update'>"
"<br><br>"
"<div id='prg'></div>"
"<br><div id='prgbar'><div id='bar'></div></div><br></form>"
"<script>"
"function sub(obj){"
"var fileName = obj.value.split('\\\\');"
"document.getElementById('file-input').innerHTML = '   '+ fileName[fileName.length-1];"
"};"
"$('form').submit(function(e){"
"e.preventDefault();"
"var form = $('#upload_form')[0];"
"var data = new FormData(form);"
"$.ajax({"
"url: '/update',"
"type: 'POST',"
"data: data,"
"contentType: false,"
"processData:false,"
"xhr: function() {"
"var xhr = new window.XMLHttpRequest();"
"xhr.upload.addEventListener('progress', function(evt) {"
"if (evt.lengthComputable) {"
"var per = evt.loaded / evt.total;"
"$('#prg').html('progress: ' + Math.round(per*100) + '%');"
"$('#bar').css('width',Math.round(per*100) + '%');"
"}"
"}, false);"
"return xhr;"
"},"
"success:function(d, s) {"
"console.log('success!') "
"},"
"error: function (a, b, c) {"
"}"
"});"
"});"
"const editor ="
"document.querySelector('grammarly-editor-plugin');"
"editor.config = {"
"suggestionCards: 'off'"
"};"
"</script>" + style;

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

#ifdef ESP8266
ESP8266HTTPUpdateServer _httpUpdater;
ESP8266WebServer _httpServer(80);
#endif

#ifdef ESP32
WebServer _httpServer(80);
#endif

String _webClientReturnString = "";
bool _updatingFirmware = false;

void handleSendToRoot()
{
	_httpServer.sendHeader("Location", "/", true); //Redirect to our html web page 
	_httpServer.send(302, "text/plane", "");
}

void setupOTA()
{
	ArduinoOTA.onStart([]() {
		_updatingFirmware = true;
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH)
			type = "sketch";
		else // U_SPIFFS
			type = "filesystem";

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		DEBUG_PRINTLN("Start updating " + type);
		});

	ArduinoOTA.onEnd([]() {
		DEBUG_PRINTLN("\nEnd");
		});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		DEBUG_PRINT("Progress: \r"); DEBUG_PRINT((progress / (total / 100))); DEBUG_PRINTLN("%");
		});
	ArduinoOTA.onError([](ota_error_t error) {
		DEBUG_PRINT("Error[%u]: "); DEBUG_PRINTLN(error);
		if (error == OTA_AUTH_ERROR) DEBUG_PRINTLN("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) DEBUG_PRINTLN("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) DEBUG_PRINTLN("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) DEBUG_PRINTLN("Receive Failed");
		else if (error == OTA_END_ERROR) DEBUG_PRINTLN("End Failed");
		delay(1000);
		ESP.restart();
		});

	ArduinoOTA.begin();
}

