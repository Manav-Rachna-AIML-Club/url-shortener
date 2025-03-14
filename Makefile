all:
	build restart status logs

build:
	./build.sh

restart:
	sudo systemctl restart urlshort

status:
	sudo systemctl status urlshort

logs:
	sudo journalctl -u urlshort --no-pager | tail -n 20

