void setup() {
	Serial.begin(9600);
	Serial.println("sensor begin");
	wdt_enable(WDTO_2S); // enable watchdog with 2s timeout. reset in ts.mainloop
	pecanInit config={.nodeId= myId, .pin1= defaultPin, .pin2= defaultPin};
	pecan_CanInit(config);
	sensorInit(&plpc, &ts);
}

void loop() {
	wdt_reset();
	while( waitPackets(&plpc) != NOT_RECEIVED);  //handle CAN messages
	ts.execute();    //Execute scheduled tasks
}