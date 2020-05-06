//-----------------------------------------------------------------------------------------------------
ModbusRtuToTcpProxy - proxy server program for converting ModbusRTU protocol to ModbusTCP.

//-----------------------------------------------------------------------------------------------------
Forked from:
The homepage for com0com project is http://com0com.sourceforge.net/.
COM port to TCP redirector. Brought to you by:
vfrolov [Vyacheslav Frolov](http://sourceforge.net/u/vfrolov/profile/)
//-----------------------------------------------------------------------------------------------------
IDE - Visual Studio 2015.

ModbusRtuToTcpProxy - программа для преобразования протокола ModbusRTU в ModbusTCP.
Является прокси сервером между программой-преобразователем интерфейсов serial COMM в TCP/IP.
Например - HW VSP3 Single(https://www.hw-group.com/software/hw-vsp3-virtual-serial-port).
Подходит для тех у кого есть программное обеспечение для звязи с устройствами по протоколу
ModbusRTU через COMM порт, но он хочет связываться с устройствами по протоколу
ModbusTCP, по сети TCP/IP.

Как это работает?
Программа HW VSP3 Single в роли клиента зоздаёт виртуальный COMM порт, принимает в него данные от
программного обеспечение для звязи с устройствами по протоколу ModbusRTU и
отправляет их в соккет для передачи по сети TCP/IP.
ModbusRtuToTcpProxy - в качестве прокси сервера принимает данные из соккета
в который их отправляет HW VSP3 Single в роли клиента,
преобразовывает протокол ModbusRTU в ModbusTCP и отправляет их в другой соккет,
для передачи по сети TCP/IP устройству ModbusTCP.

Программа HW VSP3 Single в роли сервера на другом конце сети зоздаёт виртуальный COMM порт
и отправляет в него данные принятые из соккета по сети TCP/IP.

//-----------------------------------------------------------------------------------------------------
Использование:
HW VSP3 Single.
Запустите HW VSP3 Single.
вкладка - Virtual Serial Port:
введите в поле IP Address: localhoct или 127.0.0.1
введите в поле Port: 502 или другой.
введите в поле Port Name: COM3 или другой.
создайте виртуальный COMM порт: кнопка - Create COM

ModbusRtuToTcpProxy.
напишите в командной строке: 
ModbusRtuToTcpProxy  <listen port> <host addr> <host port>
Для примера: 
ModbusRtuToTcpProxy      502      192.168.0.9     502
<listen port> - порт в который принимаются данные от - HW VSP3 Single.
<host addr> - адрес устройства ModbusTCP в сети TCP/IP.
<host port> - порт устройства ModbusTCP в сети TCP/IP.

Файл start.bat для удобного запуска программы ModbusRtuToTcpProxy.

//-----------------------------------------------------------------------------------------------------
ModbusRtuToTcpProxy - a program for converting the ModbusRTU protocol to ModbusTCP.
It is a proxy server between the serial COMM to TCP / IP converter.
For example - HW VSP3 Single (https://www.hw-group.com/software/hw-vsp3-virtual-serial-port).
Suitable for those who have protocol communication software
ModbusRTU via COMM port, but it wants to communicate with devices via protocol
ModbusTCP over TCP / IP.

How it works?
The HW VSP3 Single program as a client creates a virtual COMM port, receives data from
software for calling ModbusRTU devices and
sends them to a socket for transmission over a TCP / IP network.
ModbusRtuToTcpProxy - accepts data from a socket as a proxy server
to which HW VSP3 Single sends them as a client,
converts the ModbusRTU protocol to ModbusTCP and sends them to another socket,
for transmission over a TCP / IP network to a ModbusTCP device.

HW VSP3 Single program as a server on the other end of the network creates a virtual COMM port
and sends data received from the socket to the TCP / IP network into it.

//-----------------------------------------------------------------------------------------------------
Using:
HW VSP3 Single.
Launch HW VSP3 Single.
tab - Virtual Serial Port:
enter in the IP Address: localhoct or 127.0.0.1 field
Type in Port: 502 or another.
Type in Port Name: COM3 or another.
create virtual COMM port: button - Create COM

ModbusRtuToTcpProxy.
write on the command line:
ModbusRtuToTcpProxy <listen port> <host addr> <host port>
For example:
ModbusRtuToTcpProxy      502 192.168.0.9         502
<listen port> - port to which data is received from - HW VSP3 Single.
<host addr> is the address of the ModbusTCP device on the TCP / IP network.
<host port> - port of the ModbusTCP device on the TCP / IP network.

Start.bat file for convenient launch of the ModbusRtuToTcpProxy program.