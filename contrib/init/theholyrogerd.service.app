# It is not recommended to modify this file in-place, because it will
# be overwritten during package upgrades. If you want to add further
# options or overwrite existing ones then use
# $ systemctl edit theholyrogerd.service
# See "man systemd.service" for details.

# Note that almost all daemon options could be specified in
# /etc/theholyroger/theholyroger.conf

[Unit]
Description=TheHolyRoger daemon
After=network.target

[Service]
RuntimeDirectory=theholyrogerd
User=theholyroger
Type=forking
ExecStart=/usr/local/bin/theholyrogerd -daemon -conf=/root/.theholyroger/theholyroger.conf -pid=/run/theholyrogerd/theholyrogerd.pid
PIDFile=/run/theholyrogerd/theholyrogerd.pid
Restart=on-failure
PrivateTmp=true

[Install]
WantedBy=multi-user.target
