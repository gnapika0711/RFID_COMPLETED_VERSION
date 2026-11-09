#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>

#define SS_PIN 5
#define RST_PIN 21
#define MAX_ITEMS 12
#define MAX_HISTORY 20

MFRC522 reader(SS_PIN, RST_PIN);
WebServer server(80);
Preferences preferences;

String lastUser = "No card scanned";
String lastMessage = "Scan a registered RFID card.";
int currentUser = -1;

String userNames[3] = {"Laasya", "Gnapika", "Manu"};

String equipmentNames[MAX_ITEMS];
int totalQuantity[MAX_ITEMS];
int borrowedQuantity[MAX_ITEMS];
int itemCount = 0;

String historyUser[MAX_HISTORY];
String historyItem[MAX_HISTORY];
String historyAction[MAX_HISTORY];
int historyCount = 0;

String searchText = "";

const byte userCards[3][4] = {
  {0xAA, 0x22, 0x93, 0x02},  // Laasya
  {0x03, 0x14, 0x59, 0xAD},  // Gnapika
  {0x93, 0x23, 0x12, 0xAA}   // Manu
};

void addHistory(String user, String item, String action) {
  for (int i = MAX_HISTORY - 1; i > 0; i--) {
    historyUser[i] = historyUser[i - 1];
    historyItem[i] = historyItem[i - 1];
    historyAction[i] = historyAction[i - 1];
  }

  historyUser[0] = user;
  historyItem[0] = item;
  historyAction[0] = action;

  if (historyCount < MAX_HISTORY) {
    historyCount++;
  }

  preferences.putInt("hcount", historyCount);

  for (int i = 0; i < historyCount; i++) {
    preferences.putString(("hu" + String(i)).c_str(), historyUser[i]);
    preferences.putString(("hi" + String(i)).c_str(), historyItem[i]);
    preferences.putString(("ha" + String(i)).c_str(), historyAction[i]);
  }
}

void saveItems() {
  preferences.putInt("icount", itemCount);

  for (int i = 0; i < itemCount; i++) {
    preferences.putString(("en" + String(i)).c_str(), equipmentNames[i]);
    preferences.putInt(("et" + String(i)).c_str(), totalQuantity[i]);
    preferences.putInt(("eb" + String(i)).c_str(), borrowedQuantity[i]);
  }
}

void loadItems() {
  itemCount = preferences.getInt("icount", 0);
  historyCount = preferences.getInt("hcount", 0);

  if (itemCount > MAX_ITEMS) itemCount = MAX_ITEMS;
  if (historyCount > MAX_HISTORY) historyCount = MAX_HISTORY;

  for (int i = 0; i < itemCount; i++) {
    equipmentNames[i] = preferences.getString(("en" + String(i)).c_str(), "");
    totalQuantity[i] = preferences.getInt(("et" + String(i)).c_str(), 0);
    borrowedQuantity[i] = preferences.getInt(("eb" + String(i)).c_str(), 0);
  }

  for (int i = 0; i < historyCount; i++) {
    historyUser[i] = preferences.getString(("hu" + String(i)).c_str(), "");
    historyItem[i] = preferences.getString(("hi" + String(i)).c_str(), "");
    historyAction[i] = preferences.getString(("ha" + String(i)).c_str(), "");
  }
}

int availableQuantity(int item) {
  return totalQuantity[item] - borrowedQuantity[item];
}

String equipmentRows() {
  String rows = "";
  String searchLower = searchText;
  searchLower.toLowerCase();

  for (int i = 0; i < itemCount; i++) {
    String nameLower = equipmentNames[i];
    nameLower.toLowerCase();

    if (searchText.length() > 0 && nameLower.indexOf(searchLower) < 0) {
      continue;
    }

    String status = "<span class='badge available'>Available</span>";

    if (availableQuantity(i) == 0) {
      status = "<span class='badge empty'>Fully Borrowed</span>";
    } else if (availableQuantity(i) < 2) {
      status = "<span class='badge low'>Low Stock</span>";
    }

    String borrowedBy = "";

    for (int user = 0; user < 3; user++) {
      int userBorrowed = preferences.getInt(("u" + String(user) + "i" + String(i)).c_str(), 0);

      if (userBorrowed > 0) {
        borrowedBy += userNames[user] + " (" + String(userBorrowed) + ") ";
      }
    }

    if (borrowedBy == "") {
      borrowedBy = "None";
    }

    String action = "";

    if (currentUser >= 0 && availableQuantity(i) > 0) {
      action += "<a class='btn borrow' href='/borrow?item=" + String(i) + "'>Borrow</a>";
    }

    if (currentUser >= 0 && preferences.getInt(("u" + String(currentUser) + "i" + String(i)).c_str(), 0) > 0) {
      action += "<a class='btn return' href='/return?item=" + String(i) + "'>Return</a>";
    }

    action += "<a class='btn delete' href='/remove?item=" + String(i) + "'>Remove</a>";

    rows += "<tr>";
    rows += "<td><b>" + equipmentNames[i] + "</b><br>" + status + "</td>";
    rows += "<td>" + String(totalQuantity[i]) + "</td>";
    rows += "<td>" + String(borrowedQuantity[i]) + "</td>";
    rows += "<td>" + String(availableQuantity(i)) + "</td>";
    rows += "<td>" + borrowedBy + "</td>";
    rows += "<td>" + action + "</td>";
    rows += "</tr>";
  }

  if (rows == "") {
    rows = "<tr><td colspan='6' class='empty-row'>No equipment found.</td></tr>";
  }

  return rows;
}

String historyRows() {
  String rows = "";

  for (int i = 0; i < historyCount; i++) {
    rows += "<tr><td>" + historyUser[i] + "</td>";
    rows += "<td>" + historyItem[i] + "</td>";
    rows += "<td>" + historyAction[i] + "</td></tr>";
  }

  if (rows == "") {
    rows = "<tr><td colspan='3' class='empty-row'>No activity yet.</td></tr>";
  }

  return rows;
}

String userItems() {
  String items = "";

  if (currentUser < 0) {
    return "<p class='empty-row'>Scan a registered card to see borrowed equipment.</p>";
  }

  for (int i = 0; i < itemCount; i++) {
    int userBorrowed = preferences.getInt(("u" + String(currentUser) + "i" + String(i)).c_str(), 0);

    if (userBorrowed > 0) {
      items += "<div class='user-item'><b>" + equipmentNames[i] + "</b> - " + String(userBorrowed) + " borrowed</div>";
    }
  }

  if (items == "") {
    items = "<p class='empty-row'>You have not borrowed any equipment.</p>";
  }

  return items;
}

void handleRoot() {
  String html = "<!DOCTYPE html>"
  "<html><head>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
  "<title>Equipment Borrowing System</title>"
  "<style>"
  "*{box-sizing:border-box;}"
  "body{font-family:Arial,sans-serif;background:#f5f5f5;margin:0;padding:20px;color:#222;}"
  ".container{max-width:900px;margin:auto;}"
  ".header{background:linear-gradient(135deg,#b30000,#ff4d4d);color:white;padding:25px;border-radius:18px;text-align:center;box-shadow:0 6px 18px rgba(0,0,0,0.18);}"
  ".header h1{margin:0;font-size:28px;}"
  ".header p{margin:5px 0 0;opacity:0.9;}"
  ".card{background:white;border-radius:15px;padding:20px;margin-top:18px;box-shadow:0 4px 12px rgba(0,0,0,0.08);}"
  ".user-banner{background:#fff2f2;border-left:6px solid #b30000;padding:15px;border-radius:10px;}"
  ".user-name{font-size:22px;font-weight:bold;color:#b30000;}"
  ".message{color:#555;margin-top:5px;}"
  ".stats{display:flex;gap:12px;flex-wrap:wrap;margin-top:15px;}"
  ".stat{flex:1;min-width:130px;background:#fff2f2;padding:15px;border-radius:12px;text-align:center;}"
  ".stat h2{margin:0;color:#b30000;font-size:26px;}"
  ".stat p{margin:5px 0 0;color:#555;}"
  ".search-add{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:15px;}"
  "input{padding:10px;border:1px solid #ddd;border-radius:8px;flex:1;min-width:150px;}"
  "button,.btn{border:none;border-radius:8px;padding:10px 14px;text-decoration:none;font-size:14px;cursor:pointer;}"
  ".add-btn{background:#b30000;color:white;}"
  ".borrow{background:#b30000;color:white;}"
  ".return{background:#2e7d32;color:white;}"
  ".delete{background:#555;color:white;}"
  "table{width:100%;border-collapse:collapse;margin-top:10px;}"
  "th{background:#b30000;color:white;padding:12px;text-align:left;}"
  "td{padding:12px;border-bottom:1px solid #eee;vertical-align:top;}"
  ".badge{padding:4px 10px;border-radius:20px;font-size:12px;font-weight:bold;}"
  ".available{background:#e8f5e9;color:#2e7d32;}"
  ".low{background:#fff3e0;color:#ef6c00;}"
  ".empty{background:#ffebee;color:#b30000;}"
  ".empty-row{color:#777;text-align:center;}"
  ".user-item{background:#f9f9f9;padding:8px 10px;border-radius:8px;margin-bottom:6px;}"
  ".history td{font-size:14px;}"
  "@media(max-width:600px){th,td{padding:8px;font-size:13px;}.header h1{font-size:22px;}}"
  "</style>"
  "</head><body>"
  "<div class='container'>"

  "<div class='header'>"
  "<h1>Equipment Borrowing System</h1>"
  "<p>RFID-based lab equipment management</p>"
  "</div>"

  "<div class='card user-banner'>"
  "<div class='user-name'>" + lastUser + "</div>"
  "<div class='message'>" + lastMessage + "</div>"
  "</div>"

  "<div class='stats'>"
  "<div class='stat'><h2>" + String(itemCount) + "</h2><p>Equipment Types</p></div>"
  "<div class='stat'><h2>" + String(borrowedQuantity[0] + borrowedQuantity[1] + borrowedQuantity[2]) + "</h2><p>Borrowed Items</p></div>"
  "<div class='stat'><h2>" + String(historyCount) + "</h2><p>Recent Actions</p></div>"
  "</div>"

  "<div class='card'>"
  "<h3>My Borrowed Equipment</h3>"
  + userItems() +
  "</div>"

  "<div class='card'>"
  "<h3>Equipment</h3>"

  "<div class='search-add'>"
  "<form action='/' method='get' style='display:flex;gap:10px;flex:1;'>"
  "<input type='text' name='search' placeholder='Search equipment...' value='" + searchText + "'>"
  "<button type='submit'>Search</button>"
  "</form>"
  "<a class='btn return' href='/'>Refresh</a>"
  "</div>"

  "<form action='/add' method='get'>"
  "<div class='search-add'>"
  "<input type='text' name='name' placeholder='Equipment name' required>"
  "<input type='number' name='quantity' placeholder='Quantity' min='1' required>"
  "<button class='add-btn' type='submit'>Add Equipment</button>"
  "</div>"
  "</form>"

  "<table>"
  "<tr><th>Equipment</th><th>Total</th><th>Borrowed</th><th>Available</th><th>Borrowed By</th><th>Action</th></tr>"
  + equipmentRows() +
  "</table>"
  "</div>"

  "<div class='card'>"
  "<h3>Borrow History</h3>"
  "<table class='history'>"
  "<tr><th>User</th><th>Equipment</th><th>Action</th></tr>"
  + historyRows() +
  "</table>"
  "</div>"

  "</div>"
  "</body></html>";

  server.send(200, "text/html", html);
}

void handleSearch() {
  searchText = server.arg("search");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAdd() {
  String name = server.arg("name");
  int quantity = server.arg("quantity").toInt();

  if (name.length() > 0 && quantity > 0 && itemCount < MAX_ITEMS) {
    equipmentNames[itemCount] = name;
    totalQuantity[itemCount] = quantity;
    borrowedQuantity[itemCount] = 0;
    itemCount++;

    saveItems();
    addHistory(lastUser, name, "Added equipment");

    lastMessage = name + " added successfully.";
    Serial.println("Added equipment: " + name + ", quantity: " + String(quantity));
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleBorrow() {
  if (currentUser < 0) {
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  int item = server.arg("item").toInt();

  if (item >= 0 && item < itemCount && availableQuantity(item) > 0) {
    borrowedQuantity[item]++;

    String userKey = "u" + String(currentUser) + "i" + String(item);
    int userBorrowed = preferences.getInt(userKey.c_str(), 0);
    preferences.putInt(userKey.c_str(), userBorrowed + 1);

    saveItems();
    addHistory(userNames[currentUser], equipmentNames[item], "Borrowed 1 item");

    lastMessage = "1 " + equipmentNames[item] + " borrowed by " + userNames[currentUser] + ".";
    Serial.println("Borrowed: 1 " + equipmentNames[item] + " by " + userNames[currentUser]);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleReturn() {
  if (currentUser < 0) {
    server.sendHeader("Location", "/");
    server.send(303);
    return;
  }

  int item = server.arg("item").toInt();

  if (item >= 0 && item < itemCount && borrowedQuantity[item] > 0) {
    borrowedQuantity[item]--;

    String userKey = "u" + String(currentUser) + "i" + String(item);
    int userBorrowed = preferences.getInt(userKey.c_str(), 0);

    if (userBorrowed > 0) {
      preferences.putInt(userKey.c_str(), userBorrowed - 1);
    }

    saveItems();
    addHistory(userNames[currentUser], equipmentNames[item], "Returned 1 item");

    lastMessage = "1 " + equipmentNames[item] + " returned successfully.";
    Serial.println("Returned: 1 " + equipmentNames[item]);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRemove() {
  int item = server.arg("item").toInt();

  if (item >= 0 && item < itemCount) {
    String deletedName = equipmentNames[item];

    for (int i = item; i < itemCount - 1; i++) {
      equipmentNames[i] = equipmentNames[i + 1];
      totalQuantity[i] = totalQuantity[i + 1];
      borrowedQuantity[i] = borrowedQuantity[i + 1];
    }

    itemCount--;
    saveItems();
    addHistory(lastUser, deletedName, "Removed equipment");

    lastMessage = deletedName + " removed successfully.";
    Serial.println("Removed equipment: " + deletedName);
  }

  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  Serial.begin(115200);

  preferences.begin("equip-system", false);
  loadItems();

  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP("RFID_Project", "rfid12345")) {
    Serial.println("Could not start Wi-Fi.");
    return;
  }

  SPI.begin(18, 19, 23, SS_PIN);
  reader.PCD_Init();
  delay(100);

  server.on("/", handleRoot);
  server.on("/add", handleAdd);
  server.on("/borrow", handleBorrow);
  server.on("/return", handleReturn);
  server.on("/remove", handleRemove);
  server.begin();

  Serial.println("Equipment Borrowing System started.");
  Serial.print("Open this address in your browser: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Scan your card...");
}

void loop() {
  server.handleClient();

  if (!reader.PICC_IsNewCardPresent()) {
    return;
  }

  if (!reader.PICC_ReadCardSerial()) {
    return;
  }

  currentUser = -1;

  if (reader.uid.size == 4) {
    for (int user = 0; user < 3; user++) {
      bool matches = true;

      for (int i = 0; i < 4; i++) {
        if (reader.uid.uidByte[i] != userCards[user][i]) {
          matches = false;
          break;
        }
      }

      if (matches) {
        currentUser = user;
        break;
      }
    }
  }

  if (currentUser >= 0) {
    lastUser = userNames[currentUser];
    lastMessage = "Welcome, " + userNames[currentUser] + "! You can borrow or return equipment.";

    Serial.println("Welcome, " + userNames[currentUser] + "!");
  } else {
    lastUser = "Unknown card";
    lastMessage = "User not registered.";
    currentUser = -1;

    Serial.println("User not registered.");
  }

  reader.PICC_HaltA();
  reader.PCD_StopCrypto1();

  Serial.println("Remove the card before scanning again.");
}