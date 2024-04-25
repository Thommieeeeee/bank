const express = require("express");
const cors = require("cors");

const app = express();

const port = 8080;

app.use(express.json());
app.use(cors());

app.get('/api/noob/health', cors(), (req, res) => {
    res.json({ status: "OK" });
});

app.get('/api/accountinfo', cors(), (req, res) => {
    const { iban, pincode, pasnummer } = req.query;
    
    // Controleren of alle parameters zijn meegegeven
    if (iban && pincode && pasnummer) {
        res.sendStatus(200); // Stuur een status 200 als alle parameters zijn meegegeven
    } else {
        res.status(400).json({ error: "Ontbrekende parameters" }); // Stuur een status 400 als er parameters ontbreken
    }
});

app.post('/api/withdraw', cors(), (req, res) => {
    const { iban, pincode, pasnummer } = req.body;

    // Simulatie van opnamebedrag, hier kun je echte logica toevoegen om het bedrag te bepalen
    const amount = 50; // Stel een standaardopnamebedrag in

    // Controleer of alle parameters zijn meegegeven
    if (iban && pincode && pasnummer) {
        res.json({ amount }); // Retourneer het opnamebedrag
    } else {
        res.status(400).json({ error: "Ontbrekende parameters" }); // Stuur een status 400 als er parameters ontbreken
    }
});

app.listen(port, () => {
    console.log(`Server draait!`);
});