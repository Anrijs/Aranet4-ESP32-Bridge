#ifndef __AR4BR_UTILS_H
#define __AR4BR_UTILS_H

int ip2str(uint32_t ip, char* buf) {
    return sprintf(buf, "%u.%u.%u.%u",
        ip >> 24 & 0xFF,
        ip >> 16 & 0xFF,
        ip >> 8  & 0xFF,
        ip       & 0xFF);
}

uint32_t str2ip(String str) {
    if (str.length() < 7 || str.length() > 15) {
        return 0; // Invalid IP
    }
  
    char raw[16];
    str.toCharArray(raw, str.length() + 1);
  
    uint32_t ip = 0;
    uint8_t ip_byte = 3;
  
    char buf[4];
    uint8_t buf_pos = 0;
  
    memset(buf, 0, 4);
  
    for (uint8_t i=0; i<str.length() + 1; i++) {
        char c = raw[i];
        if (c == '.' || c == 0) {
            if (buf_pos == 0) {
              return 0; // Invalid IP
            }
      
            ip |=  atoi(buf) << (ip_byte*8);
            --ip_byte;
            buf_pos = 0;
            memset(buf, 0, 4);
        } else if (c >= '0' && c <= '9') {
            buf[buf_pos++] = c;
            if (buf_pos > 3) {
              return 0; // Invalid IP
            }
        } else {
            return 0; // Invalid IP
        }
    }
  
    return ip;
}

uint8_t hex2int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

int mac2str(uint8_t *mac, char* buf, bool reverse) {
    if (reverse) {
        return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[5],mac[4],mac[3],mac[2],mac[1],mac[0]
        );
    } else {
        return sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]
        );
    }
}

uint8_t str2mac(String str, uint8_t *mac, bool reverse) {
    memset(mac, 0, 6);
  
    if (str.length() != 17) {
        return 0;
    }
  
    char raw[18];
    str.toCharArray(raw, 18);

    for (uint8_t i = 0; i< 6; i++) {
        uint8_t pos = i * 3;
        if (reverse) {
            mac[5-i] = ((hex2int(raw[pos]) << 4) | hex2int(raw[pos + 1]));
        } else {
            mac[i] = ((hex2int(raw[pos]) << 4) | hex2int(raw[pos + 1]));
        }
    }

    return 1;
}

#endif
