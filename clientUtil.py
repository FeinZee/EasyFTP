import socket
import sys

from PyQt5.QtCore import pyqtSignal, QObject

BUF_SIZE = 1024


class error_reply(ValueError): pass


class error_other(ValueError): pass


class FTP(QObject):
    isPassive = 1
    host = "localhost"  # will be changed by PASV
    port = 21
    listen_sock = socket.socket()  # for port
    cmd_sock = socket.socket()
    data_sock = socket.socket()
    rw_offset = 0
    transfering_file = ''
    paused = False
    response_sig = pyqtSignal(str)
    error_sig = pyqtSignal(str)

    def __init__(self):
        super().__init__()

    # get response from command socket and strip CRLF
    def get_response(self):
        recv_data = (self.cmd_sock.recv(BUF_SIZE)).decode("utf-8")
        if not recv_data:
            raise EOFError
        if recv_data[-2:] == '\r\n':
            recv_data = recv_data[:-2]
        elif recv_data[-1:] in '\r\n':
            recv_data = recv_data[:-1]
        return recv_data

    # 创建套接字、连接服务器
    def connect_establish(self, ip, port):
        try:
            HOST = ip
            PORT = port
            ADDR = (HOST, PORT)
            print(ADDR)
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(ADDR)
            self.cmd_sock = sock
            recv_data = self.get_response()
            code = recv_data[0:3]
            if code == "220":
                print(recv_data)
                self.response_sig.emit(recv_data)
            else:
                print("服务器表示不欢迎")
                self.error_sig.emit(recv_data)
        except Exception as e:
            print("连接服务器出错：")
            print(e)
            self.error_sig.emit("连接服务器出错")


    def cmd_user(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_pass(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def pasv_connect(self):
        conn = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        addr = (self.host, self.port)
        conn.connect(addr)
        return conn

    def cmd_retr(self, param):

        if self.isPassive == 1:
            conn = self.pasv_connect()
            resp = self.get_response()  # suppose to be 150
            print(resp)
            if resp[0:3] != "150":
                self.error_sig.emit(resp)
                return
            else:
                self.response_sig.emit(resp)
                size = self.parse_file_size(resp)  # file size (bytes)

            tar_file = open(param, mode='wb')
            if tar_file is None:
                print("local file created or opened failed")
                self.error_sig.emit("local file created or opened failed")

            while 1:
                if self.paused:
                    self.response_sig.emit("transfer file paused")
                    break
                data = conn.recv(BUF_SIZE)
                if len(data) <= 0:
                    self.rw_offset = 0
                    self.transfering_file = ''
                    break
                self.rw_offset += len(data)
                tar_file.write(data)

            tar_file.close()
            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig.emit(resp)
            else :
                self.error_sig.emit(resp)
            conn.close()
        elif self.isPassive == 0:  # port
            self.data_sock = self.listen_sock.accept()
            conn = self.data_sock
            resp = self.get_response()  # suppose to be 150
            print(resp)
            if resp[0:3] != "150":
                self.error_sig.emit(resp)
                return
            else:
                self.response_sig.emit(resp)

            tar_file = open(param, mode='wb')
            if tar_file is None:
                print("local file created or opened failed")
                self.error_sig.emit("local file created or opened failed")
            while 1:
                if self.paused:
                    self.response_sig.emit("transfer file paused")
                    break
                data = conn.recv(BUF_SIZE)
                if len(data) <= 0:
                    self.rw_offset = 0
                    self.transfering_file = ''
                    break
                self.rw_offset += len(data)
                tar_file.write(data)

            tar_file.close()
            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig.emit(resp)
            else :
                self.error_sig.emit(resp)
            self.data_sock.close()

    def cmd_stor(self, param):
        if self.isPassive == 1:
            self.data_sock = self.pasv_connect()
            resp = self.get_response()  # suppose to be 150
            print(resp)
            if resp[0:3] != "150":
                self.error_sig.emit(resp)
                return
            else:
                self.response_sig.emit(resp)
                size = self.parse_file_size(resp)  # file size (bytes)

            tar_file = open(param, mode='rb')
            if tar_file is None:
                print('no such local file')
                self.error_sig.emit("no such local file")
                return
            self.transfering_file = param
            while 1:
                if self.paused:
                    self.response_sig.emit("transfer file paused")
                    break
                data = tar_file.read(BUF_SIZE)
                if len(data) <= 0:
                    self.rw_offset = 0
                    self.transfering_file = ''
                    break
                self.rw_offset += len(data)
                self.data_sock.send(data)
                # 影响进度条


            tar_file.close()
            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig(resp)
            else:
                self.error_sig(resp)
            self.data_sock.close()

        elif self.isPassive == 0:  # port
            self.data_sock = self.listen_sock.accept()
            resp = self.get_response()  # suppose to be 150
            conn = self.data_sock
            print(resp)
            if resp[0:3] != "150":
                self.error_sig.emit(resp)
                return
            else:
                self.response_sig.emit(resp)
                size = self.parse_file_size(resp)  # file size (bytes)

            tar_file = open(param, mode='rb')
            if tar_file is None:
                print('no such local file')
                self.error_sig.emit("no such local file")
                return
            self.transfering_file = param
            while 1:
                if self.paused:
                    self.response_sig.emit("transfer file paused")
                    break

                data = tar_file.read(BUF_SIZE)
                if len(data) <= 0:
                    self.rw_offset = 0
                    self.transfering_file = ''
                    break
                self.rw_offset += len(data)
                self.data_sock.send(data)
                # 影响进度条


            tar_file.close()
            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig(resp)
            else:
                self.error_sig(resp)
            self.data_sock.close()
            pass
        pass

    def cmd_quit(self):
        resp = self.get_response()
        self.response_sig(resp)
        print(resp)

    def cmd_syst(self):
        resp = self.get_response()
        self.response_sig(resp)
        print(resp)

    def cmd_type(self):
        resp = self.get_response()
        self.response_sig(resp)
        print(resp)

    # param
    def cmd_port(self, param, cmdLine):
        ip_port = param.split(',')
        new_port = int(ip_port[-2]) * 256 + int(ip_port[-1])
        new_host = '.'.join(ip_port[0:4])

        listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        addr = (new_host, new_port)
        listen_sock.bind(addr)
        listen_sock.listen(1)

        self.cmd_sock.send(cmdLine.encode("utf-8"))
        resp = self.get_response()
        print(resp)

        if resp[:3] != '200':
            self.error_sig(resp)
            raise error_reply(resp)
        else:
            self.response_sig(resp)
            self.isPassive = 0
            self.host = new_host
            self.port = new_port
            self.listen_sock = listen_sock
            return new_host, new_port, listen_sock

    def cmd_pasv(self):
        resp = self.get_response()
        print(resp)
        if resp[:3] != '227':
            self.error_sig(resp)
            raise error_reply(resp)
        else:
            self.response_sig(resp)
            self.host, self.port = self.parse_pasv_resp(resp)
            self.isPassive = 1

    def cmd_mkd(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_cwd(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_pwd(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_list(self):
        if self.isPassive == 1:
            conn = self.pasv_connect()
            resp = self.get_response()  # suppose to be 150
            print(resp)
            if resp[0:3] != "150":
                self.error_sig(resp)
                return
            else:
                self.response_sig(resp)

            while 1:
                data = conn.recv(BUF_SIZE)
                if not data:
                    break
                # 制作到目录
                print("list: ", data)
                self.response_sig(data)

            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig(resp)
            else:
                self.error_sig(resp)
            conn.close()
        elif self.isPassive == 0:  # port
            conn = self.listen_sock.accept()
            resp = self.get_response()  # suppose to be 150
            print(resp)
            if resp[0:3] != "150":
                self.error_sig(resp)
                return
            else:
                self.response_sig(resp)

            while 1:
                data = conn.recv(BUF_SIZE)
                if not data:
                    break
                # 制作到目录
                print("list: ", data)
                self.response_sig(data)

            resp = self.get_response()
            print(resp)
            if resp[0:3] == "226":
                self.response_sig(resp)
            else:
                self.error_sig(resp)
            self.listen_sock.close()

    def cmd_rmd(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_rnfo(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def cmd_rnto(self):
        resp = self.get_response()
        print(resp)
        self.response_sig.emit(resp)

    def send_cmd(self, cmdLine):
        cmdLineElement = cmdLine.partition(" ")
        CMD = cmdLineElement[0]
        # 全部大写
        CMD = CMD.upper()
        PARAM = cmdLineElement[2]
        # 如果没有参数，PARAM == ""
        cmdLine = CMD + " " + PARAM + "\r\n"
        if CMD == "PORT":
            self.cmd_port(PARAM, cmdLine)
            return
        self.cmd_sock.send(cmdLine.encode("utf-8"))
        if CMD == "USER":
            self.cmd_user()
        elif CMD == "PASS":
            self.cmd_pass()
        elif CMD == "PASV":
            self.cmd_pasv()
        elif CMD == "RETR":
            self.cmd_retr(PARAM)
        elif CMD == "STOR":
            self.cmd_retr(PARAM)
        elif CMD == "SYST":
            self.cmd_syst()
        elif CMD == "TYPE":
            self.cmd_type()
        elif CMD == "MKD":
            self.cmd_mkd()
        elif CMD == "CWD":
            self.cmd_cwd()
        elif CMD == "PWD":
            self.cmd_pwd()
        elif CMD == "RMD":
            self.cmd_rmd()
        elif CMD == "RNFR":
            self.cmd_rnfr()
        elif CMD == "RNTO":
            self.cmd_rnto()
        elif CMD == "LIST":
            self.cmd_list()
        elif CMD == "QUIT":
            self.cmd_quit()
        else:
            msg = self.get_response()
            print(msg)
            self.error_sig.emit(msg)

    _227_re = None

    # get the host and port from the response of PASV
    def parse_pasv_resp(self, resp):
        if self._227_re is None:
            import re
            self._227_re = re.compile(r'(\d+),(\d+),(\d+),(\d+),(\d+),(\d+)', re.ASCII)
        m = self._227_re.search(resp)
        if not m:
            raise error_other(resp)
        numbers = m.groups()
        host = '.'.join(numbers[:4])
        port = (int(numbers[4]) << 8) + int(numbers[5])
        return host, port

    _150_re = None

    def parse_file_size(self, resp):
        '''
        Returns the expected transfer size or None; size is not guaranteed to
        be present in the 150 message.
        '''
        if resp[:3] != '150':
            raise error_reply(resp)
        if self._150_re is None:
            import re
            self._150_re = re.compile(
                r"150 .* \((\d+) bytes\)", re.IGNORECASE | re.ASCII)
        m = self._150_re.match(resp)
        if not m:
            return None
        return int(m.group(1))


'''
if __name__ == "__main__":
    
    ftp = FTP()
    while 1:

        cmdLine = input("请输入命令：")
        
'''
