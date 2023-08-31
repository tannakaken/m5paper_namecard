#include <M5EPD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include <iterator>

#include "Free_Fonts.h"

M5EPD_Canvas canvas(&M5.EPD);
M5EPD_Canvas novelCanvas(&M5.EPD);

/**
 * 
 * @brief uth8の文字をどのバイトで切り取れば与えられた長さになるかを決める関数。下記のサイトを参考にした
 * @see https://blog.sarabande.jp/post/64272610753
 * 
 * @param str 切り取る文字列
 * @param target_size 切り取る長さ
 * @return そのバイトで切り取ればstrがsizeになるバイト数。strが短すぎた場合は-1 
 */
int utf8_target_bytesize(String str, uint32_t target_size)
{
    using namespace std;

    unsigned int len = 0;

    for (int pos = 0; pos < str.length(); /* 何もしない */) {

        unsigned char c = str[pos];
        int char_size = 0;
        if (c < 0x80) {
            char_size = 1;
        } else if (c < 0xE0) {
            char_size = 2;
        } else if (c < 0xF0) {
            char_size = 3;
        } else {
            char_size = 4;
        }

        len += 1;
        pos += char_size;
        if (len == target_size) {
          return pos;
        }
    }

    return -1;
}

static const uint32_t linesize = 13;
static const uint32_t lineoffsetx = 20;
static const uint32_t lineoffsety = 10;
static const uint32_t lineHeight = 42;

String newTitle;
String title;
std::vector<String> newLines;
std::vector<String> lines;
int startLine = 0;
static const int lineLimit = 22;

void prepareNovel() {
  title = newTitle;
  lines.clear();
  copy(newLines.begin(), newLines.end(), back_inserter(lines));
}

void getNovel() {
  HTTPClient http;
  String payload;
  http.begin("https://tannakaken.xyz/api/novels/name_card/chapters/random");
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
    }
  }
  DynamicJsonDocument doc(2048);
  auto error = deserializeJson(doc, payload);
  if (error != DeserializationError::Ok) {
    Serial.println("fail deserialization json");
    Serial.println(error.c_str());
  }
  newTitle = doc["title"].as<String>();

  String body = doc["body"];
  newLines.clear();
  while (true) {
    int nl = body.indexOf('\n');
    if (nl == -1) {
      break;
    }
    String rest = body.substring(0, nl);
    rest = "　" + rest;
    body = body.substring(nl+1);
    int target_bytesize = 0;
    while ((target_bytesize = utf8_target_bytesize(rest, linesize)) != -1) {
      String line = rest.substring(0, target_bytesize);
      rest = rest.substring(target_bytesize);
      if (rest.startsWith("、")) {
        line = line + "、";
        rest = rest.substring(String("、").length());
      } else if (rest.startsWith("。")) {
        line = line + "。";
        rest = rest.substring(String("。").length());
      } else if (rest.startsWith("ー")) {
        line = line + "ー";
        rest = rest.substring(String("ー").length());
      } else if (rest.startsWith("っ")) { // ちいさい「つ」
        line = line + "っ";
        rest = rest.substring(String("っ").length());
      } else if (rest.startsWith("ッ")) {
        line = line + "ッ";
        rest = rest.substring(String("ッ").length());
      } else if (rest.startsWith("」")) {
        line = line + "」";
        rest = rest.substring(String("」").length());
      }
      newLines.push_back(line);
    }
    if (rest.length() > 0) {
      newLines.push_back(rest);
    }
  }
  Serial.println("got");
}

void showNovel() {
  novelCanvas.clear();
  int i = 0;
  for (String line: lines) {
    if (i >= startLine) {
      novelCanvas.drawString(line, lineoffsetx, lineoffsety + (i - startLine) * lineHeight);
    }
    i++;
    if ((i - startLine) == lineLimit) {
      break;
    }
  }
  M5.EPD.Clear(true); // 画面を切り替える前にclearしないと前の表示が残る
  novelCanvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
}

bool wifiConnected = false;

void prepareNameCard() {
  canvas.loadFont("/ipaexm.ttf", SD);
  canvas.createRender(96); // 後で使うサイズであらかじめpre-renderingしておく。二つ目の引数でキャッシュサイズが指定でき、パフォーマンスを改善できる。
  canvas.createRender(56); // これをしないと、このサイズのフォントを表示しようとしても、エラーが出て何も表示できない。
  String buf;
  
  canvas.setTextDatum(TL_DATUM);
  canvas.fillCanvas(0);

  canvas.setTextSize(56);
  buf = "小説家"; 
  canvas.drawString(buf, 12, 64);
  buf = "淡中 圏";
  canvas.setTextSize(96);
  canvas.drawString(buf, 12, 140);
  
  canvas.fillCircle(270, 640, 200, WHITE); // WHITEで黒く表示される
  canvas.fillCircle(270, 640, 180, BLACK); // BLACKで白く表示される
  
  canvas.setTextSize(56);
  buf = "名刺小説";
  canvas.drawString(buf, 150, 595);
  buf = "を読む";
  canvas.drawString(buf, 180, 660);

  canvas.loadFont("/ipaexg.ttf", SD);
  canvas.createRender(40);

  canvas.setTextSize(40);
  buf = "アプリ作成";
  canvas.drawString(buf, 22, 240);
  buf = "プロトタイピング";
  canvas.drawString(buf, 22, 290);
  buf = "データ分析";
  canvas.drawString(buf, 22, 340);
  buf ="などなどもやります。";
  canvas.drawString(buf, 22, 390);

  canvas.unloadFont(); // 日本語フォントで英字を表示するとほぼ読めないのでアンロードする。

  buf = "tannakaken@gmail.com";
  canvas.setTextSize(3);
  canvas.drawString(buf, 12, 16);

  if (wifiConnected) {
    canvas.drawString("wifi", 12, 900);
  } else {
    canvas.drawString("no wifi", 12, 900);
  }
}

void setup()
{
  M5.begin();
  Serial.begin(9600);
  M5.EPD.SetRotation(90);
  M5.EPD.Clear(true);
  M5.RTC.begin();
  canvas.createCanvas(540, 960);
  novelCanvas.createCanvas(540, 960);
  canvas.setTextDatum(TC_DATUM);
  canvas.setTextSize(3);
  if (!SD.begin()) {                                        // SDカードの初期化
    Serial.println("Card failed, or not present");          // SDカードが未挿入の場合の出力
    canvas.drawString("Card failed, or not present", 270, 640);
    canvas.pushCanvas(0,0,UPDATE_MODE_DU4);
    while (1);
  }
  canvas.drawString("... Initializing ...", 270, 640);
  canvas.pushCanvas(0,0,UPDATE_MODE_DU4);

  if (SD.exists("/wifi.txt")) {
    Serial.println("wifi.txt exists");
    File wifiFile = SD.open("/wifi.txt", FILE_READ);
    if (wifiFile) {
      String ssid = wifiFile.readStringUntil('\n');
      String password = wifiFile.readStringUntil('\n');
      WiFi.begin(ssid.c_str(), password.c_str());
      int retry_limit = 100;
      int retry = 0;
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.println(".");
        retry++;
        if (retry_limit == retry) {
          break;
        }
      }
      if (retry_limit != retry) {
        wifiConnected = true;
      }
      
    }
  }

  prepareNameCard();
  M5.EPD.Clear(true);
  canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);

  novelCanvas.loadFont("/ipaexg.ttf", SD);
  novelCanvas.createRender(38);
  novelCanvas.setTextSize(38);
  
  getNovel();
}

bool isNovelMode = false;
bool gotNovel = true;

void loop()
{
  if (isNovelMode && !gotNovel) {
    getNovel();
    gotNovel = true;
  }
  M5.update();
  if (M5.BtnR.wasPressed()) { // BtnRは縦持ちだと下向き
    startLine += 16;
    showNovel();
  } else if (M5.BtnL.wasPressed()) { // BtnLは縦持ちだと上向き
    if (startLine > 0) {
      startLine -= 16;
      showNovel();
    }
  } else if (M5.BtnP.wasPressed()) { // BtnPは押す
    M5.EPD.Clear(true);
    canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
    isNovelMode = false;
  }
  if (!M5.TP.avaliable()) {
    return;
  }
  static int lastFingers = 0;
  static uint16_t fingerX = 0;
  static uint16_t fingerY = 0;
  M5.TP.update();
  if (M5.TP.isFingerUp()) {
    if (lastFingers == 1) {
      if (isNovelMode) {
        if (fingerY < 100) {
          Serial.println("up");
          if (startLine == 0) {
            canvas.pushCanvas(0, 0, UPDATE_MODE_DU4);
            isNovelMode = false;
          } else {
            startLine -= 4;
            showNovel();
          }
        }
        else if (fingerY > 900) {
          Serial.println("down");
          startLine += 4;
          showNovel();
        }
      } else {
        int diffX = fingerX - 270;
        int diffY = fingerY - 640;
        int square_distance = diffX * diffX + diffY * diffY;
        if (square_distance < 180 * 180) {
          Serial.println("touch");
          prepareNovel();
          startLine = 0;
          showNovel();
          gotNovel = false;
          isNovelMode = true;
        }
      }
    }
    lastFingers = 0;
  } else {
    lastFingers = M5.TP.getFingerNum();
    if (lastFingers == 1) {
      // スクリーンを縦に使う場合はタッチの座標の取得はxとyが逆になるので注意
      fingerX = M5.TP.readFingerY(0);
      fingerY = M5.TP.readFingerX(0);
    }
  }

}