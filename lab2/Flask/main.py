
from flask import Flask, render_template

app = Flask(__name__)


@app.route('/')
def index():
    return 'welcome to my webpage!'


@app.route('/user.html')
def newspage():
    return render_template("user.html")

if __name__=="__main__":
    app.run(port=8080, host="127.0.0.1")