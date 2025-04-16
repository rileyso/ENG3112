# server.py
from flask import Flask, request
import datetime

app = Flask(__name__)

@app.route('/post_result', methods=['POST'])
def post_result():
    data = request.get_json()
    print(f"[{datetime.datetime.now()}] Received: {data}")
    return {'status': 'ok'}

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
