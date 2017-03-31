
pia-resolv-fix: fix.c
	cc -Wall -Os -o pia-resolv-fix fix.c

clean:
	rm pia-resolv-fix

install: pia-resolv-fix pia-resolv-fix.service
	cp pia-resolv-fix /usr/local/bin
	cp pia-resolv-fix.service /etc/systemd/system/
	systemctl daemon-reload
	systemctl enable pia-resolv-fix.service
	systemctl restart pia-resolv-fix

uninstall:
	systemctl stop pia-resolv-fix.service
	systemctl disable pia-resolv-fix.service
	rm -f /usr/local/bin/pia-resolv-fix
	rm -f /etc/systemd/system/pia-resolv-fix.service
