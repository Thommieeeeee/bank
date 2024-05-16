const express = require("express");
const cors = require("cors");
const bodyParser = require('body-parser');

const app = express();

const port = 8080;

app.use(express.json());
app.use(cors());
app.use(bodyParser.json());

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
        const userData = {
            first_name: "John",
            last_name: "Doe",
            balance: 1000.00
        };
        res.status(200).json(userData);
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