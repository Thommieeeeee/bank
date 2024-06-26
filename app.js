const express = require("express");
const cors = require("cors");
const bodyParser = require('body-parser');
const mysql = require('mysql2');
const http = require('http');
const WebSocket = require('ws');
const path = require('path');

const app = express();

const port = 8080;

// Stel Express in om de website directory te serveren
const publicPath = path.resolve(__dirname, '..', '..', 'website');

app.use(express.json());
app.use(cors());
app.use(bodyParser.json());
app.use(express.static(publicPath));

let db;
const maxRetries = 10;
const retryDelay = 5000;
let retryCount = 0;

let espClient = null;  // Variable to store the ESP client
let webClient = null;

// Voeg een GET-route toe voor de root URL
app.get('/', (req, res) => {
    res.sendFile(path.join(publicPath, 'tempindex.html'));
});

// Maak een HTTP-server
const server = http.createServer(app);

// Maak een WebSocket-server
const wss = new WebSocket.Server({ server });

function connectToDatabase() {
    db = mysql.createConnection({
        host: process.env.DB_HOST || 'localhost',
        user: process.env.DB_USER || 'api',
        password: process.env.DB_PASSWORD || 'password',
        database: process.env.DB_NAME || 'BOV'
    });

    db.connect((err) => {
        if (err) {
            console.error(`Database connection error: ${err.message}`);
            if (retryCount < maxRetries) {
                retryCount++;
                console.log(`Retrying database connection (${retryCount}/${maxRetries})...`);
                setTimeout(connectToDatabase, retryDelay); // Retry after delay
            } else {
                console.error('Max retries reached. Exiting.');
                process.exit(1);
            }
        } else {
            console.log('Connected to the database.');
            startServer();
        }
    });
}

function startServer() {
    app.get('/api/noob/health', cors(), (req, res) => {
        res.json({ status: "OK" });
    });

    app.post('/api/accountinfo', cors(), (req, res) => {
        const { target } = req.query;
        const { pincode, uid } = req.body;
        const token = req.header('NOOB-TOKEN');

        console.log(target, pincode, uid, token);

        // Controleren of alle parameters zijn meegegeven
        if (target.match(/^ZW\d{2}BANB\d{10}/)) {
            // Als alle parameters aanwezig zijn, stuur een JSON-reactie met de gebruikersgegevens
            const query = 'SELECT firstname, lastname, balance FROM clients WHERE client_id = ? AND pincode = ?';
            db.query(query, [target, pincode], (err, results) => {
                if (err) {
                    console.error(`Database query error: ${err.message}`);
                    res.status(500).json({ error: "Internal Server Error" });
                } else if (results.length === 0) {
                    res.status(404).json({ error: "Client not found or incorrect pincode" });
                } else {
                    const userData = results[0];

                    // Check if the balance field exists and is a string
                    if (userData.balance && typeof userData.balance === 'string') {
                        // Parse the balance field to an integer
                        userData.balance = parseInt(userData.balance, 10);
                    }

                    console.log(userData);
                    res.status(200).json(userData);
                }
            });
        } else {
            fetch("https://noob.datalabrotterdam.nl/api/noob/accountinfo" + '?target=' + target, {
                method: "POST",
                body: JSON.stringify(req.body),
                headers: {
                    "Content-type": "application/json; charset=UTF-8",
                    "NOOB-TOKEN": "f022be6b-e8ba-4470-83d0-3269534f3b8b"
                }
            })
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    // Read the response body as json
                    return response.json();
                })
                .then(data => {
                    // Log the response body
                    console.log(data);
                    res.status(200).json(data);
                })
                .catch(error => {
                    console.error('There was a problem with the fetch operation:', error);
                });

            //res.status(400).json({ error: "Ontbrekende parameters" }); // Stuur een status 400 als er parameters ontbreken
        }
    });

    app.post('/api/withdraw', cors(), (req, res) => {
        const { target } = req.query;
        const { amount, pincode, uid } = req.body;
        const token = req.header('NOOB-TOKEN');

        // Controleer of alle parameters zijn meegegeven
        if (target.match(/^ZW\d{2}BANB\d{10}/)) {
            res.status(200).json({ amount }); // Retourneer het opnamebedrag
        } else {
            fetch("https://noob.datalabrotterdam.nl/api/noob/withdraw" + '?target=' + target, {
                method: "POST",
                body: JSON.stringify(req.body),
                headers: {
                    "Content-type": "application/json; charset=UTF-8",
                    "NOOB-TOKEN": "f022be6b-e8ba-4470-83d0-3269534f3b8b"
                }
            })
                .then(response => {
                    // Check if response was successful
                    if (response.ok) {
                        // Log the response body if needed
                        console.log("Request successful");
                        // Return status code
                        return response.status;
                    } else {
                        // Log the full error message for debugging
                        console.error('Network response was not ok:', response.status);
                        throw new Error('Network response was not ok');
                    }
                })
                .then(status => {
                    // Send the status code as response
                    res.status(status).send();
                })
                .catch(error => {
                    console.error('There was a problem with the fetch operation:', error);
                    // Handle error response
                    res.status(500).json({ error: 'Internal Server Error' });
                });
        }
    });

    app.get('/api/time', cors(), (req, res) => {
        const query = 'SELECT DATE_FORMAT(NOW(), "%Y-%m-%d %H:%i:%s") as currentTime';
        db.query(query, (err, results) => {
            if (err) {
                console.error(`Database query error: ${err.message}`);
                res.status(500).json({ error: "Internal Server Error" });
            } else {
                const currentTime = results[0].currentTime;
                res.status(200).json({ currentTime });
            }
        });
    });

    // app.listen(port, "145.24.223.83", () => {
    //     console.log(`Server draait!`);
    // });
};

wss.on('connection', ws => {
    console.log('Nieuwe client verbonden');

    ws.on('message', message => {
        console.log(`Received message: ${message}`);

        if (message == 'Hallo van de client!') {
            webClient = ws;
            console.log('Web client verbonden');
            return;
        }

        if (message == 'ESP_CONNECTED') {
            espClient = ws;
            console.log('ESP client verbonden');
            return;
        }

        if (message == 'Return') {
            if (espClient) {
                espClient.send('Return');
                console.log('Return bericht naar ESP gestuurd');
            } else {
                console.log('Geen ESP client verbonden om het bericht te ontvangen');
            }
            return;
        }

        if (message == 'Bon') {
            if (espClient) {
                espClient.send('Bon');
                console.log('Bon bericht naar ESP gestuurd');
            } else {
                console.log('Geen ESP client verbonden om het bericht te ontvangen');
            }
            return;
        }

        if (message == 'Geen bon') {
            if (espClient) {
                espClient.send('Geen bon');
                console.log('Geen bon bericht naar ESP gestuurd');
            } else {
                console.log('Geen ESP client verbonden om het bericht te ontvangen');
            }
            return;
        }

        // Broadcast the message to all connected clients except the sender
        wss.clients.forEach(client => {
            if (client !== ws && client.readyState === WebSocket.OPEN) {
                // If the message is being sent to the espClient, ensure it's sent as a string
                if (client == espClient) {
                    try {
                        const jsonMessage = JSON.stringify(JSON.parse(message)); // Ensure it's valid JSON and stringify it
                        client.send(jsonMessage);
                    } catch (e) {
                        client.send(message); // If not JSON, send as is
                    }
                } else {
                    client.send(message);
                }
                console.log('Bericht doorgestuurd naar clients');
            }
        });
    });

    ws.on('close', () => {
        if (ws == espClient) {
            espClient = null;  // Reset the ESP client when it disconnects
            console.log('ESP client heeft de verbinding verbroken');
        }
        if (ws == webClient) {
            webClient = null;
            console.log('Web client heeft de verbinding verbroken');
        }
    });

    ws.send('Welkom nieuwe client!');
});


// Laat de websocketserver luisteren op poort 8080
server.listen(port, "145.24.223.83", () => {
    console.log(`WebsocketServer draait!`);
});

connectToDatabase();