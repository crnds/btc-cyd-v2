SKETCH := firmware/btc_ticker
FQBN   := esp32:esp32:esp32:PartitionScheme=huge_app,UploadSpeed=115200
PORT   := /dev/cu.usbserial-110
BAUD   := 115200

.PHONY: build flash monitor clean

build:
	arduino-cli compile --fqbn $(FQBN) $(SKETCH)

flash:
	arduino-cli compile --fqbn $(FQBN) --upload -p $(PORT) $(SKETCH)

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

clean:
	rm -rf $(SKETCH)/build
