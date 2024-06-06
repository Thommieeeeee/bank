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
        console.log(`Ontvangen bericht: ${message}`);
        // Stuur het ontvangen bericht door naar alle verbonden clients
        wss.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(message);
            }
        });
    });

    ws.on('close', () => {
        console.log('Client heeft verbinding verbroken');
    });

    ws.send('Welkom nieuwe client!');
});

// Laat de websocketserver luisteren op poort 8080
server.listen(port, "145.24.223.83", () => {
    console.log(`WebsocketServer draait!`);
});

connectToDatabase();