import cv2
import time
import socket 

ESP32_IP = "192.168.254.208"
STREAM_URL = f'http://{ESP32_IP}/stream'
UDP_PORT = 12345           
FACE_CASCADE_PATH = 'haarcascade_frontalface_default.xml'

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_address = (ESP32_IP, UDP_PORT)
print(f"Enviando comandos UDP para {ESP32_IP}:{UDP_PORT}")

pan_angle = 90
tilt_angle = 90
PAN_P_GAIN = 0.08
TILT_P_GAIN = 0.1

def send_command_udp(pan, tilt):
    """Envia os ângulos dos servos para o ESP32 via UDP."""
    try:
        pan = max(0, min(180, int(pan)))
        tilt = max(0, min(180, int(tilt)))
        
        command_string = f"X{pan}Y{tilt}"
        
        sock.sendto(command_string.encode(), server_address)

    except Exception as e:
        print(f"Erro ao enviar comando UDP: {e}")
        
face_cascade = cv2.CascadeClassifier(FACE_CASCADE_PATH)
if face_cascade.empty():
    print("Erro: Não foi possível carregar o classificador Haar Cascade.")
    exit()

cap = cv2.VideoCapture(STREAM_URL)
if not cap.isOpened():
    print(f"Erro: Não foi possível abrir o stream de vídeo em {STREAM_URL}.")
    exit()

print("Conexões bem-sucedidas! Iniciando rastreamento...")
time.sleep(2) 

send_command_udp(pan_angle, tilt_angle)

try:
    while True:
        ret, frame = cap.read()
        if not ret:
            print("Stream encerrado ou erro na leitura do frame.")
            break

        (frame_h, frame_w) = frame.shape[:2]
        center_x_frame = frame_w // 2
        center_y_frame = frame_h // 2
        
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(40, 40))

        if len(faces) > 0:
            (x, y, w, h) = max(faces, key=lambda item: item[2] * item[3])
            cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)

            face_center_x = x + w // 2
            face_center_y = y + h // 2

            error_x = center_x_frame - face_center_x
            error_y = center_y_frame - face_center_y

            pan_angle += error_x * PAN_P_GAIN
            tilt_angle -= error_y * TILT_P_GAIN
            
            send_command_udp(pan_angle, tilt_angle)

        cv2.imshow('Face Tracking', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break
finally:
    print("Encerrando o programa.")
    send_command_udp(90, 90) 
    sock.close()
    cap.release()
    cv2.destroyAllWindows()
