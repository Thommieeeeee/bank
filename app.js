const express = require("express");
const cors = require("cors");
const bodyParser = require('body-parser');
const mysql = require('mysql2');

const app = express();

const port = 8080;

app.use(express.json());
app.use(cors());
app.use(bodyParser.json());

let db;
const maxRetries = 10;
const retryDelay = 5000;
let retryCount = 0;

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
        console.log('help');
        const { target } = req.query;
        const { pincode, uid } = req.body;
        const token = req.header('NOOB-TOKEN');

        console.log(target, pincode, uid, token);

        // Controleren of alle parameters zijn meegegeven
        if (target.match(/^ZW\d{2}BANB\d{10}/)) {
            // Als alle parameters aanwezig zijn, stuur een JSON-reactie met de gebruikersgegevens
            const query = 'SELECT name, balance FROM clients WHERE client_id = ? AND pincode = ?';
            db.query(query, [target, pincode], (err, results) => {
                if (err) {
                    console.error(`Database query error: ${err.message}`);
                    res.status(500).json({ error: "Internal Server Error" });
                } else if (results.length === 0) {
                    res.status(404).json({ error: "Client not found or incorrect pincode" });
                } else {
                    const userData = results[0];
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
        const { pincode, uid, amount } = req.body;
        const token = req.header('NOOB-TOKEN');

        // Controleer of alle parameters zijn meegegeven
        if (target.match(/^ZW\d{2}BANB\d{10}/)) {
            res.status(200).json({ amount }); // Retourneer het opnamebedrag
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

    app.listen(port, "145.24.223.83", () => {
        console.log(`Server draait!`);
    });
};

connectToDatabase();