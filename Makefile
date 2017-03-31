
pia-resolv-fix: fix.c
	cc -Wall -Os -o pia-resolv-fix fix.c

clean:
	rm pia-resolv-fix

install: pia-resolv-fix cronjob
	cp pia-resolv-fix /usr/local/bin
	cp cronjob /etc/cron.d/pia-resolv-fix

uninstall:
	rm -f /usr/local/bin/pia-resolv-fix
	rm -f /etc/cron.d/pia-resolv-fix
