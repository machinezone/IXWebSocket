FROM python:3.8.0-alpine3.10

RUN pip install websockets
COPY ws_proxy.py /usr/bin
RUN chmod +x /usr/bin/ws_proxy.py

EXPOSE 8765
CMD ["python", "/usr/bin/ws_proxy.py"]
