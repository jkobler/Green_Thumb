

#include <map>
#include <Arduino.h>
#include <WiFi.h>


namespace jsonParser {

    typedef struct {
        String fieldName;
        String fieldKey;
        String fieldValue;
        String* parentFieldName;
        uint8_t level;
    } field;

    std::map<String, field> fields;

    void parseJSON(WiFiClient &client) {
        int length;
        uint8_t level=-1;
        String* parentFieldName;
        bool inFeedBlock = false;
        uint8_t feedCount = 0;

        while (client.connected()) {
            String line = client.readStringUntil('\n');
            int cl = line.indexOf("Content-Length: ");
            if (cl > -1) {
                String lengthStr = line.substring(cl+17);
                lengthStr.trim();
                length = lengthStr.toInt(); 
            }
            if (line == "\r") break;
        }



        while (client.connected() && client.available()) {
            String line = client.readStringUntil('\n');

            if (line.indexOf("{") > -1) level++;
            if (line.indexOf("}") > -1) level--;

            if (line.indexOf("]") > -1) inFeedBlock = false;
   

            if (line.indexOf("\"feeds\":") > -1) 
                inFeedBlock = true;
            else if (inFeedBlock == true) {
                if (line.indexOf('{')) feedCount++;
                else {
                    




                }
            }
        }
    }
    



    


}




