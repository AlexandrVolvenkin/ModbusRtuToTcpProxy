//-----------------------------------------------------------------------------------------------------
ModbusRtuToTcpProxy - proxy server program for converting ModbusRTU protocol to ModbusTCP.

//-----------------------------------------------------------------------------------------------------
Forked from:
The homepage for com0com project is http://com0com.sourceforge.net/.
COM port to TCP redirector. Brought to you by:
vfrolov [Vyacheslav Frolov](http://sourceforge.net/u/vfrolov/profile/)
//-----------------------------------------------------------------------------------------------------
IDE - Visual Studio 2015.

ModbusRtuToTcpProxy - ��������� ��� �������������� ��������� ModbusRTU � ModbusTCP.
�������� ������ �������� ����� ����������-���������������� ����������� serial COMM � TCP/IP.
�������� - HW VSP3 Single(https://www.hw-group.com/software/hw-vsp3-virtual-serial-port).
�������� ��� ��� � ���� ���� ����������� ����������� ��� ����� � ������������ �� ���������
ModbusRTU ����� COMM ����, �� �� ����� ����������� � ������������ �� ���������
ModbusTCP, �� ���� TCP/IP.

��� ��� ��������?
��������� HW VSP3 Single � ���� ������� ������ ����������� COMM ����, ��������� � ���� ������ ��
������������ ����������� ��� ����� � ������������ �� ��������� ModbusRTU �
���������� �� � ������ ��� �������� �� ���� TCP/IP.
ModbusRtuToTcpProxy - � �������� ������ ������� ��������� ������ �� �������
� ������� �� ���������� HW VSP3 Single � ���� �������,
��������������� �������� ModbusRTU � ModbusTCP � ���������� �� � ������ ������,
��� �������� �� ���� TCP/IP ���������� ModbusTCP.

��������� HW VSP3 Single � ���� ������� �� ������ ����� ���� ������ ����������� COMM ����
� ���������� � ���� ������ �������� �� ������� �� ���� TCP/IP.

//-----------------------------------------------------------------------------------------------------
�������������: �������� � ��������� ������: 
ModbusRtuToTcpProxy  <listen port> <host addr> <host port>
��� �������: 
ModbusRtuToTcpProxy      502      192.168.0.9     502

���� start.bat ��� �������� ������� ��������� ModbusRtuToTcpProxy.

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
Usage: write on the command line:
ModbusRtuToTcpProxy <listen port> <host addr> <host port>
For example:
ModbusRtuToTcpProxy     502      192.168.0.9      502

Start.bat file for convenient launch of the ModbusRtuToTcpProxy program.
