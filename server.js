const express = require('express');
const bodyParser = require('body-parser');
const mysql = require('mysql2');
const app = express();
const port = 3000;

app.use(bodyParser.json());

let db;
const maxRetries = 10;  // Increase retries
const retryDelay = 5000; // Increase delay to 5 seconds
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
    app.post('/login', (req, res) => {
        const { client_id, pin } = req.body;

        const query = 'SELECT * FROM clients WHERE client_id = ? AND pincode = ?';
        db.query(query, [client_id, pin], (err, results) => {
            if (err) {
                console.error('Error executing query:', err);
                res.status(500).json({ success: false, message: 'An error occurred while processing your request.' });
                return;
            }

            if (results.length > 0) {
                res.json({ success: true });
            } else {
                res.status(401).json({ success: false, message: 'Invalid credentials.' });
            }
        });
    });

    app.post('/lock_account', (req, res) => {
        const { clientId } = req.body;

        const query = 'UPDATE clients SET status = "inactive" WHERE client_id = ?';
        db.query(query, [clientId], (err, results) => {
            if (err) {
                console.error('Error executing query:', err);
                res.status(500).json({ success: false, message: 'An error occurred while processing your request.' });
                return;
            }

            res.json({ success: true, message: 'Account locked successfully.' });
        });
    });

    app.get('/summary', (req, res) => {
        const { client_id } = req.query;

        const query = 'SELECT name as client_name, client_id, balance FROM clients WHERE client_id = ?';
        db.query(query, [client_id], (err, results) => {
            if (err) {
                console.error('Error executing query:', err);
                res.status(500).json({ success: false, message: 'An error occurred while fetching client details.' });
                return;
            }

            if (results.length > 0) {
                const client = results[0]; // Assuming client_id is unique
                res.json({ success: true, client });
            } else {
                res.status(404).json({ success: false, message: 'Client not found.' });
            }
        });
    });

    app.listen(port, () => {
        console.log(`Server running at http://localhost:${port}/`);
    });
}

connectToDatabase();
