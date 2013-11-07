# Remote deploy (Use keys to avoid password prompts!)
SSH_IP = 192.168.1.118
SSH_USER = rohit
SSH_DIR = /home/rohit/cse536



USER_APP=project3app
DEVICE= /dev/cse5361


DRIVER_PATH = linux-3.5.0/drivers/char/cse536

all: $(USER_APP)

$(USER_APP): $(USER_APP).c udpclient.c udpserver.c
	gcc  -pthread -g -ggdb  $< -o $(USER_APP)
	gcc udpclient.c -o udpclient
	gcc udpserver.c -o udpserver


module:
	make -C linux-3.5.0/ M=drivers/char/cse536 modules

install:  module
	-sudo mknod $(DEVICE) c 234 0
	sudo chown rohit $(DEVICE)
	sudo chmod 777 $(DEVICE)
	sudo insmod \
		$(DRIVER_PATH)/cse5361.ko debug_enable=1
	

load:
	@echo "Copying to Remote Machine..."
	scp  udpserver $(USER_APP)  $(DRIVER_PATH)/cse5361.ko  $(SSH_USER)@$(SSH_IP):$(SSH_DIR)
	
	@echo "Done!"

run: $(USER_APP) 
	clear
	./$(USER_APP)

clean:
	sudo rmmod cse5361
	rm $(USER_APP) udpclient udpserver
	sudo rm -rf $(DEVICE)
	
