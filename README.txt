1. Compilar o programa
$ mkdir build
$ cd build
$ cmake ..
$ make

2. Mover o binário compilado para a pasta principal
$ cp uploader-monitor /home/jet/uploader-monitor/

3. Copiar o arquivo service
$ sudo cp /home/jet/uploader-monitor/uploader-monitor.service /etc/systemd/system/

4. Ativar e iniciar o serviço
$ sudo systemctl daemon-reload
$ sudo systemctl enable uploader-monitor
$ sudo systemctl start uploader-monitor

5. Verificar o status
$ sudo systemctl status uploader-monitor

6. Ver logs em tempo real
$ sudo journalctl -u uploader-monitor -f
