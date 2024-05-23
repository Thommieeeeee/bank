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
        const { target } = req.query;
        const { pincode, uid } = req.body;
        const token = req.header('NOOB-TOKEN');
        
        // Controleren of alle parameters zijn meegegeven
        if (target && pincode && uid && token) {
            // Als alle parameters aanwezig zijn, stuur een JSON-reactie met de gebruikersgegevens
            const query = 'SELECT name, balance FROM clients WHERE client_id = ? AND pincode = ?';
            db.query(query, [101, 123456], (err, results) => {
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
            res.status(400).json({ error: "Ontbrekende parameters" }); // Stuur een status 400 als er parameters ontbreken
        }
    });

    app.post('/api/withdraw', cors(), (req, res) => {
        const { target } = req.query;
        const { pincode, uid, amount } = req.body;
        const token = req.header('NOOB-TOKEN');

        // Controleer of alle parameters zijn meegegeven
        if (target && pincode && uid && amount && token) {
            res.status(200).json({ amount }); // Retourneer het opnamebedrag
        } else {
            res.status(400).json({ error: "Ontbrekende parameters" }); // Stuur een status 400 als er parameters ontbreken
        }
    });

    app.listen(port, "145.24.223.83", () => {
        console.log(`Server draait!`);
    });
};

connectToDatabase();