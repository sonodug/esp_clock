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
    msg.days = dayValue; msg.hours = hoursValue; msg.minutes = minutesValue;
    msg.seconds = secondsValue; Socket.send(JSON.stringify(msg));
}
function processCommand(event) {
    var obj = JSON.parse(event.data);
    document.getElementById('temperature').innerHTML = obj.temperature;
    document.getElementById('humidity').innerHTML = obj.humidity;
    console.log(obj.temperature); console.log(obj.humidity);
}
window.onload = function (event) {
    init();
}