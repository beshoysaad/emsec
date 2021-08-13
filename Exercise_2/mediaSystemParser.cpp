/*
[...]
*/
unsigned short nibble2c(char c){
	if ((c>='0') && (c<='9'))
		return c-'0' ;
	if ((c>='A') && (c<='F'))
		return c+10-'A' ;
	if ((c>='a') && (c<='f')){
		return c+10-'a' ;
	}
	  
	return -1 ;
}

void error(String st, unsigned int port){
	Serial.print("Sending error message: ");
	Serial.println(st);
	char buf[st.length()+1]; 
	st.toCharArray(buf,st.length()+1);
	Udp.beginPacket(Udp.remoteIP(), port);
	Udp.write(buf);
	Udp.endPacket();
}

void parseIntoCanBuf(unsigned short *id, char canBuf[], char udpBuf[], int len){
	int rl = len;
	rl = rl/2;
	*id = nibble2c(udpBuf[0])*16*16+nibble2c(udpBuf[1])*16+nibble2c(udpBuf[2]);
	if(udpBuf[3]!='#'){
		error("Delimiter wrong format",DEBUGPORT);
	}
	for(int i = 4; i<len; i++){
		if(i%2==0){
			canBuf[i/2-2] = nibble2c(udpBuf[i])*16;
		}
		else{
			canBuf[i/2-2] += nibble2c(udpBuf[i]);
		}
	}
}


int allowed[] = {0x50,0x280,0x320,0x420,0x470,0x520,0x531,0x621,0x635,0x727,0x1a0,0x3d0,0x52a,0x5a0,0x5c1,0x5d2,0x60e,0x62e,0xc2};
char canBuf[] = {0,0,0,0,0,0,0,0,0,0,0,0};

void loop()
{  
	char udpBuf[200];
	unsigned short id = 0;
	int packetSize = Udp.parsePacket();
	if (packetSize) {
		unsigned short payloadSize = packetSize -4;
		Serial.print("Received packet of size ");
		Serial.println(packetSize);
		Serial.print("From ");
		IPAddress remote = Udp.remoteIP();
		for (int i = 0; i < 4; i++) {
			Serial.print(remote[i], DEC);
			if (i < 3) {
				Serial.print(".");
			}
		}
		Serial.print(", port ");
		Serial.println(Udp.remotePort());

		Udp.read(udpBuf, 200);

		parseIntoCanBuf(&id,canBuf,udpBuf,packetSize);
		
		boolean found = false;
		for(int i = 0;i<(sizeof(allowed)/sizeof(int));i++){
			if(id==allowed[i])
				found = true;
		}
		
		if (!found){
			String st = "ID fail: only {";
			boolean first = true;
			for(int i = 0;i<(sizeof(allowed)/sizeof(int));i++){
				if(first){
					st += allowed[i];
					first = false;
				}
				else{
					st += ", ";
					st += allowed[i];
				}
			}
			st += "} is allowed";
			error(st, DEBUGPORT);
		}
		
		engine.send(id, payloadSize/2-4, canBuf);

	}
}

/*
[...]
*/
