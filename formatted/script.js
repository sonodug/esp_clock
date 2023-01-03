var Socket;
const form = document.getElementById('form');
form.addEventListener('submit', button_set);
function init() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    Socket.onmessage = function (event) {
        processCommand(event);
    };
}
function button_set() {
    var dayValue = document.getElementById('days').value;
    var hoursValue = document.getElementById('hours').value;
    var minutesValue = document.getElementById('minutes').value;
    var secondsValue = document.getElementById('seconds').value;
    var msg = { days: '', hours: '', minutes: '', seconds: '' };
    msg.days = dayValue;
    msg.hours = hoursValue;
    msg.minutes = minutesValue;
    msg.seconds = secondsValue;
    Socket.send(JSON.stringify(msg));
}
function processCommand(event) {
    var obj = JSON.parse(event.data);
    document.getElementById('temperature').innerHTML = obj.temperature;
    document.getElementById('humidity').innerHTML = obj.humidity;
    console.log(obj.temperature);
    console.log(obj.humidity);
}
function syncTimeEsp() {
    now = new Date();
    y = now.getFullYear();
    m = now.getMonth() + 1;
    d = now.getDate();
    h = now.getHours();
    min = now.getMinutes();
    s = now.getSeconds();
    dw = now.getDay();
    var msg = { year: '', month: '', day: '', hour: '', minutes: '', seconds: '', dayOfWeek: '' };
    msg.year = y;
    msg.month = m;
    msg.day = d;
    msg.hour = h;
    msg.minutes = min;
    msg.seconds = s;
    msg.dayOfWeek = dw;
    Socket.send(JSON.stringify(msg));

    setTimeout(syncTimeEsp, 1000);
}
function outputTime() {
    now = new Date();
    d = now.getDate();
    m = now.getMonth() + 1;
    y = now.getFullYear();
    h = now.getHours();
    min = now.getMinutes();
    s = now.getSeconds();

    if (d <= 9 && m > 9) {
        document.getElementById('time').innerHTML =
            '0' + d + '.' + m + '.' + y + ' | '
            + h + ':' + min + ':' + s;
    }
    else if (d <= 9 && m <= 9) {
        document.getElementById('time').innerHTML =
            '0' + d + '.' + '0' + m + '.' + y + ' | '
            + h + ':' + min + ':' + s;
    }
    else {
        document.getElementById('time').innerHTML =
            d + '.' + m + '.' + y + ' | '
            + h + ':' + min + ':' + s;
    }

    setTimeout(outputTime, 1000);
}

outputTime();
syncTimeEsp();

window.onload = function (event) {
    init();
}