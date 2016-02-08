#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <FS.h>

int FSTotal;
int FSUsed;
const char *ssid = "3dsgateway";
const char *password = "3dsgateway";

//holds the current upload
File UploadFile;
String fileName;

//-------------- FSBrowser application -----------
//format bytes
String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

ESP8266WebServer  server(80);
WiFiClient client;

void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.print("Configuring access point...");
  WiFi.softAP(ssid, password);
  WiFi.mode(WIFI_AP);

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Check if SPIFFS is OK
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS failed, needs formatting");
    handleFormat();
    delay(500);
    ESP.restart();
  }
  else
  {
    FSInfo fs_info;
    if (!SPIFFS.info(fs_info))
    {
      Serial.println("fs_info failed");
    }
    else
    {
      FSTotal = fs_info.totalBytes;
      FSUsed = fs_info.usedBytes;
    }
  }

  // upload file to SPIFFS
  server.on("/fupload2", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");    
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START)
    {
      fileName = upload.filename;
      Serial.setDebugOutput(true);
        Serial.println("Upload Name: " + fileName);
        String path;
        path = "/" + fileName;
        UploadFile = SPIFFS.open(path, "w");
        // already existing file will be overwritten!
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (UploadFile)
          UploadFile.write(upload.buf, upload.currentSize);
        Serial.println(fileName + " size: " + upload.currentSize);
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        Serial.print("Upload Size: ");
        Serial.println(upload.totalSize);  // need 2 commands to work!
        if (UploadFile)
          UploadFile.close();
    }
      yield();
  });

  server.on ( "/format", handleFormat );
  server.on("/upload", handle_fupload_html);
  server.onNotFound([]() {
    if (!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);
  //ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );
 
  server.begin();
  Serial.println("HTTP server started");
}

void handle_index()
{
  String hostheader = server.header("User-Agent");
  
}

String getContentType(String filename) {
  if (server.hasArg("download")) return "application/octet-stream";
  else if (filename.endsWith(".htm")) return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".png")) return "image/png";
  else if (filename.endsWith(".gif")) return "image/gif";
  else if (filename.endsWith(".jpg")) return "image/jpeg";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".xml")) return "text/xml";
  else if (filename.endsWith(".pdf")) return "application/x-pdf";
  else if (filename.endsWith(".zip")) return "application/x-zip";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path)
{
  String hostheader = server.header("User-Agent"); 
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
  {
    if (hostheader.substring(13, 25) == "Nintendo 3DS")   // detect if agent is a Nintento 3DS
    {
      if (hostheader.substring(44, 50) == "1.7567")
      {
        path = "/17567_fw71to94_index.html";
      }
      if (hostheader.substring(44, 50) == "1.7552")
      {
        path = "/17552_fw50to70_index.html";
      }
      if (hostheader.substring(44, 50) == "1.7498")
      {
        path = "/17498_fw40_index.html";
      }
      if (hostheader.substring(44, 50) == "1.7455")
      {
        path = "/17455_fw21_index.html";
      }
      if (hostheader.substring(44, 50) == "1.7412")
      {
        path = "/payload_17412_fw20_index.html";
      }
    }
    else
    {
      path = "/wrong_user_index.html";
    }
  }
  
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path))
  {
    if (SPIFFS.exists(pathWithGz))
    path += ".gz";      
    File file = SPIFFS.open(path, "r");
    server.sendHeader("Connection", "close");
    size_t sent = server.streamFile(file, contentType);
    size_t contentLength = file.size();
    file.close();
    return true;
  }
  else
  {
    Serial.println("Not found: ");
    Serial.println(path);
  }
  return false;
}

void handle_fupload_html()
{  
  String HTML = "<br>Files on flash:<br>";
  Dir dir = SPIFFS.openDir("/");
  while (dir.next())
  {
    fileName = dir.fileName();
    size_t fileSize = dir.fileSize();
    HTML += fileName.c_str();
    HTML += " ";
    HTML += formatBytes(fileSize).c_str();
    HTML += " , ";
    HTML += fileSize;
    HTML += "<br>";
  }
  server.send ( 200, "text/html", "<form method='POST' action='/fupload2' enctype='multipart/form-data'><input type='file' name='update' multiple><input type='submit' value='Update'></form><br<b>For webfiles only!!</b>Multiple files possible<br>" + HTML);
}


void loop (void)
{
  server.handleClient();
}


void handleFormat()
{
  server.send ( 200, "text/html", "OK");
  Serial.println("Format SPIFFS");
  if (SPIFFS.format())
  {
    if (!SPIFFS.begin())
    {
      Serial.println("Format SPIFFS failed");
    }
  }
  else
  {
    Serial.println("Format SPIFFS failed");
  }
  if (!SPIFFS.begin())
  {
    Serial.println("SPIFFS failed, needs formatting");
  }
  else
  {
    Serial.println("SPIFFS mounted");
  }
}

void handleFileDelete()
{
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  if (!path.startsWith("/")) path = "/" + path;
  Serial.println("handleFileDelete: " + path);
  if (path == "/")
  return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
  return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

