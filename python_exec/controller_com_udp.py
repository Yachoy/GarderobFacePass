import socket

###
# protocol of send data - \~{char of command}\!{task id; negative num - ignore}\`{for arguments}
# a - add user; arg - bytes rfid
# g, b, r - blink green, blue, red; arg - count blink
# b - block all rfid cards; arg - true/false
# e - make empty storage rfid; arg - *empty*
#
###


class RGBDiode:
    sock: socket.socket = None
    udp_args: tuple[str, int] = None

    def __init__(self, static_ip, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_args = (static_ip, port)

    def green(self, blink_count: int = 3):
        print(f"rgb: green blink {blink_count}")
        self.sock.sendto(f"\~g\!-1\`{blink_count}".encode(), self.udp_args)

    def blue(self, blink_count: int = 3):
        print(f"rgb: blue blink {blink_count}")
        self.sock.sendto(f"\~b\!-1\`{blink_count}".encode(), self.udp_args)

    def red(self, blink_count: int = 3):
        print(f"rgb: red blink {blink_count}")
        self.sock.sendto(f"\~r\!-1\`{blink_count}".encode(), self.udp_args)


class StepMotor:
    sock: socket.socket = None
    udp_args: tuple[str, int] = None

    def __init__(self, static_ip, port):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp_args = (static_ip, port)

    def rotate_to(self, _id_item):
        self.sock.sendto(f"\~m\!-2\`{_id_item}".encode(), self.udp_args)


