feather.replace();




var connectionCheckInterval = window.setInterval(function () {
    checkConnection();
}, 2000);

var connection = new WebSocket('ws://' + location.hostname + ':81/', ['arduino']);

connection.onopen = function () {

    let answerJson = {
        'connected': true
    }
    sendJSON(answerJson)
};
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
    console.log("Message received:" + e.data);
    processWebsocketMessage(e.data);
}
connection.onclose = function () {
    checkConnection();
    console.log('WebSocket connection closed');
};

function sendJSON(message) {
    // input : message object
    messageString = JSON.stringify(message);
    console.log("sendJSON():");
    console.log(messageString);
    connection.send(messageString);
}

function checkConnection() {
    // 0	CONNECTING	Socket has been created. The connection is not yet open.
    // 1	OPEN	The connection is open and ready to communicate.
    // 2	CLOSING	The connection is in the process of closing.
    // 3	CLOSED	The connection is closed or couldn't be opened.
    switch (connection.readyState) {
        case 0:
            document.getElementById('connectionStatus').innerHTML = "Websocket: <b style='color: gray'>Connecting...</b>";
            document.getElementById('settings').style.display = "none";
            break;
        case 1:
            document.getElementById('connectionStatus').innerHTML = "Websocket: <b style='color: green'>Connected</b>";
            document.getElementById('settings').style.display = "block";
            // requestStatusReport(); // request report to fill UI with current data
            break;
        case 2:
            document.getElementById('connectionStatus').innerHTML = "Websocket: <b style='color: gray'>Closing...</b>";
            document.getElementById('settings').style.display = "none";
            break;
        case 3:
            document.getElementById('connectionStatus').innerHTML = "Websocket: <b style='color: gray'>Disconnected</b>";
            document.getElementById('settings').style.display = "none";
            break;
    }

}