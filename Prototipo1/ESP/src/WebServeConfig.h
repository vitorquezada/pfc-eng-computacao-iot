#ifndef WEB_SERVER_H_
#define WEB_SERVER_H_

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
  <head>
    <title>ESP Web Server</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="icon" href="data:,">
    <style>
      html {font-family: Arial; display: inline-block; text-align: center;}
      h2 {font-size: 3.0rem;}
      p {font-size: 3.0rem;}
      body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
      .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #b30000}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    </style>
  </head>
  <body>
    <h2>ESP Web Server</h2>

    <form action="/" method="post">
      <label for="mqttClientId">MQTT Client ID:</label><br>
      <input type="text" id="mqttClientId" name="mqttClientId" value="%mqttClientId%"><br>

      <label for="mqttToken">MQTT Token:</label><br>
      <input type="text" id="mqttToken" name="mqttToken" value="%mqttToken%"><br>

      <label for="wifiSsid">Wifi Ssid:</label><br>
      <input type="text" id="wifiSsid" name="wifiSsid" value="%wifiSsid%"><br>

      <label for="wifiPass">Wifi Password:</label><br>
      <input type="text" id="wifiPass" name="wifiPass" value="%wifiPass%"><br>

      <br/><br/>

      <input type="submit" value="Submit">
    </form> 

    <script>
      function toggleCheckbox(element) {
        var xhr = new XMLHttpRequest();
        if(element.checked){
          xhr.open("GET", "/update?output="+element.id+"&state=1", true);
        } else {
          xhr.open("GET", "/update?output="+element.id+"&state=0", true);
        }
        xhr.send();
      }
    </script>
  </body>
</html>
)rawliteral";

#endif /* WEB_SERVER_H_ */