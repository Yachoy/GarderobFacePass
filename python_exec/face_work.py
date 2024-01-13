import time

import cv2
import numpy as np

import os
import json
import socket
from deepface.commons import functions, realtime, distance as dst
from PIL import Image
from deepface import DeepFace
from threading import Thread
from datetime import datetime

from controller_com_udp import StepMotor, RGBDiode

face_cascade = cv2.CascadeClassifier('haarcascade_frontalface_default.xml')
capture: cv2.VideoCapture = cv2.VideoCapture()
capture.set(cv2.CAP_PROP_BUFFERSIZE, 0)


class ProcessingImageFromCameras:
    db: "FaceDataBase" = None
    cm: StepMotor = None

    list_of_free_ids: list[int] = []

    def __init__(self, db: "FaceDataBase", rgb: RGBDiode, sm: StepMotor):
        self.db = db
        self.rgb = rgb
        self.sm = sm
        for i in range(1, 9):
            self.list_of_free_ids.append(i)
        self.list_of_free_ids.reverse()

    def accept_img(self, img):
        extra, ans = self.db.find_person(img)

        if ans and extra["verified"]:
            self.db.del_person(ans["_id"])
            self.list_of_free_ids.append(ans["_id"])
            print(f"Open id {ans['_id']}")
            self.rgb.green(3)
            self.sm.rotate_to(ans["_id"])
        elif len(self.list_of_free_ids) > 0:
            _id = self.list_of_free_ids.pop(0)
            self.db.save_person(img, _id, extra_data={
                "_id": _id
            })
            print(f"Add new id {_id}")
            self.rgb.blue(3)
        else:
            print("Not found or can't reg new")
            self.rgb.red(3)


class FaceDataBase:
    def __init__(self, files_load: str, files_temp: str):
        self.data = {}
        self.filesL = files_load
        self.filesT = files_temp
        #self.reload_data_base_from_files()

    @staticmethod
    def get_vector_face(img):
        return DeepFace.represent(img, model_name="Facenet")

    def reload_data_base_from_files(self):
        data = {}
        file_path = os.path.join(self.filesL, 'extra.json')
        if not os.path.exists(file_path):
            with open(file_path, 'w') as f:
                f.write("{}")
        # Проходим по всем файлам в папке
        for filename in os.listdir(self.filesL):
            # Пропускаем файл extra.json
            if filename == 'extra.json':
                continue

            # Открываем изображение
            img = Image.open(os.path.join(self.filesL, filename))

            # Загружаем extra_data из файла extra.json
            with open(os.path.join(self.filesL, 'extra.json'), 'r') as f:
                extra_data = json.load(f).get(filename, {})

            # Добавляем данные в словарь
            data[filename] = {
                'face': img,
                'name': filename,
                'extra_data': extra_data
            }

        self.data = data

    def get_data(self) -> dict: return self.data

    def del_person(self, name: str):
        del self.data[name]

    def save_person(self, img_path: str, name: str, extra_data=None):
        img = Image.fromarray(img_path).convert("L")

        # Сохранение лица в базе данных
        self.data[name] = {
            'face': img,
            'name': name,
            'extra_data': extra_data,
            'times_visit': list()
        }

        # Сохранение фотографии в файле
        img_path_save = os.path.join(self.filesL, f'{name}.jpg')
        img.save(img_path_save)
        # Сохранение дополнительных данных в файле JSON
        with open(os.path.join(self.filesL, 'extra.json'), 'r') as f:
            data_extra = json.load(f)

        data_extra[name] = extra_data

        with open(os.path.join(self.filesL, 'extra.json'), 'w') as f:
            json.dump(data_extra, f)

    def find_person(self, img_path: str | np.ndarray, threshold: float = 0.30):
        # Чтение изображения
        path_search = self.filesT+'/temp_search.jpg'
        path_check = self.filesT+'/temp_check.jpg'
        if not isinstance(img_path, str):
            Image.fromarray(img_path).save(path_search)
            img_path = path_search

        # Поиск лица в базе данных
        for name, data in self.data.items():
            n = np.array(data["face"])
            Image.fromarray(n).save(path_check)
            result = DeepFace.verify(img1_path=img_path, img2_path=path_check, enforce_detection=False) # model_name="Facenet"
            print(f"process face by {result['time']}, res {result['distance']}")
            if result["distance"] <= threshold:
                return result, data['extra_data']

        return False, None



ip_esp = "192.168.1.X"
local_ip_this_device = "192.168.1.X"
port = 9669

esp_listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
esp_listener.bind((local_ip_this_device, port))

b_wait = 20
a_wait = 5
ignore_faces = 0

fdb = FaceDataBase("fdTelegram/", "fdTemp/")

sm = StepMotor(ip_esp, port)
rgb = RGBDiode(ip_esp, port)

processing = ProcessingImageFromCameras(fdb, rgb, sm)

pull_data = []


def lst_btn():
    while True:
        data, address = esp_listener.recvfrom(4096)
        print("pressed btn on esp")
        pull_data.append(data)

#
# for i in range(1,9):
#     sm.rotate_to(i)
#     time.sleep(1.5)


WORK = False


def start(local_camera: bool = False):
    listener_btn = Thread(
        target=lst_btn
    ); listener_btn.start()
    while True:
        if not local_camera:
            url = input("enter camera url: ")
            if url == "": break
            capture.open(url)
        else:
            capture = cv2.VideoCapture(0)
        while True:
            ret, frame = capture.read()
            if not WORK: cv2.putText(frame,
                'In waiting for press btn...',
                (50, 50),
                cv2.FONT_HERSHEY_SIMPLEX, 1,
                (0, 255, 255),
                2,
                cv2.LINE_4)
            if len(pull_data) != 0:
                pull_data.pop()
                WORK = True

            if ret and WORK:
                gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
                if ignore_faces <= 0:
                    faces = face_cascade.detectMultiScale(gray, scaleFactor=1.2, minNeighbors=5, minSize=(50, 50))
                    ignore_faces = a_wait
                    for (x, y, w, h) in faces:
                        cv2.rectangle(frame, (x, y), (x+w+10, y+h+10), (255, 0, 0), 2)
                        face_only = gray[y:y+h+10, x:x+w+10]
                        processing.accept_img(face_only)
                        ignore_faces = b_wait
                        WORK = False
                        break
                else:
                    ignore_faces -= 1
            if ret:
                cv2.imshow("App", frame)
            k = cv2.waitKey(1)
            if k % 256 == 27:
                # ESC pressed
                print("Escape hit, closing window...")
                capture.release()
                cv2.destroyAllWindows()
                break
