# TASK — ReceiptLedger fase 3

## Scope en uitgangspunten

Voeg `pdf2db` toe voor verwerking naar MySQL. Behoud `pdf2cvs` en de bestaande werking. Hergebruik de OCR-, interpretatie- en financiële controlelogica.

De gebruiker levert circa 30 PDF-tickets onder `<repo-root>/tickets`.

Alle gedeelde databasestructuur en noodzakelijke basisgegevens worden geleverd via `*.sql` onder `<repo-root>/DB/`. Een bestaande lokale database is geen vereiste.

Vink taken pas af na implementatie en passende verificatie. Vermeld bij oplevering wat getest is, het resultaat en concrete beperkingen.

Een financiële match bewijst geen correcte OCR. Ontbrekende waarden blijven onbekend. Onopgeloste bonnen komen niet als `done` in definitieve tabellen en hun PDF wordt niet opgeruimd.

## F3.1 — Databaseopbouw via SQL-scripts

- [ ] F3.1a — Maak `<repo-root>/DB/` met genummerde SQL-scripts en een duidelijke uitvoervolgorde voor volledige databaseopbouw.
- [ ] F3.1b — Definieer werk-/opbouwtabellen voor importdocumenten, verwerkingspogingen, OCR, bonkandidaten, artikelwaarnemingen, matchvoorstellen, controles en reviewbeslissingen.
- [ ] F3.1c — Definieer definitieve tabellen voor winkels, bevestigde winkelartikels, goedgekeurde bonnen, bonregels, correcties en btw-groepen.
- [ ] F3.1d — Voorzie sleutels, relaties, indexen en uniciteitsregels. Bewaar document-, pagina-, bon-, artikel- en bronregionidentiteit en gebruik een documenthash tegen dubbele import.
- [ ] F3.1e — Houd de schemaversie bij. Lever volgende schemawijzigingen als aanvullende migratiescripts met behoud van bestaande gegevens. Destructieve resets vallen buiten de normale opbouw.
- [ ] F3.1f — Lever deelbare winkelprofielen, herkenningsregels en bevestigde artikelbasisgegevens via afzonderlijke SQL-scripts. Voorzie hiervoor een reproduceerbare export, zodat bevestigde kennis uit een lokale database deelbaar wordt. Exporteer geen wachtwoorden, lokale paden of persoonlijke aankoopgegevens.
- [ ] F3.1g — Documenteer de ondersteunde MySQL-versie, vereiste rechten, tekencodering, scriptvolgorde en voorbeeldcommando’s. Alle noodzakelijke databasewijzigingen worden via SQL-scripts geleverd.
- [ ] F3.1h — Test volledige opbouw op een lege database en migratie van een bestaande versie met behoud van gegevens. Leg het gedrag bij opnieuw uitvoeren en gedeeltelijk mislukte scripts vast.

## F3.2 — App en databaseverbinding

- [ ] F3.2a — Voeg `app/pdf2db` toe volgens de bestaande module- en CMake-opbouw. Houd CLI, verwerking en opslag gescheiden van herbruikbare OCR- en boninterpretatie.
- [ ] F3.2b — Ondersteun één PDF, meerdere PDF’s en een map als invoer, met voorspelbare sortering en duidelijke foutafhandeling.
- [ ] F3.2c — Voorzie configureerbare MySQL-verbindingen zonder wachtwoorden in broncode of logs.
- [ ] F3.2d — Controleer vóór verwerking of de databaseschemaversie ondersteund wordt. Meld ontbrekende of verouderde schema’s duidelijk en begin dan niet aan gedeeltelijke verwerking.
- [ ] F3.2e — Registreer documenthash en verwerkingspogingen. Herken eerder geïmporteerde documenten en voorkom dubbele definitieve aankopen bij herhaalde of onderbroken verwerking.

## F3.3 — Winkelherkenning en folderindeling

- [ ] F3.3a — Bouw winkelprofielen met herkenningskenmerken zoals naam, adres, btw-nummer en bonkop.
- [ ] F3.3b — Herken de winkel per PDF en bewaar gebruikte aanwijzingen, alternatieven en onzekerheid. Onbekende of ambigue herkenning vereist review.
- [ ] F3.3c — Gebruik een gekende winkelfolder als winkelcontext. Signaleer tegenstrijdige bongegevens; overschrijf die niet stilzwijgend.
- [ ] F3.3d — Lever een controleerbaar voorstel om PDF’s in winkelfolders te verdelen. Verplaats pas na bevestiging en behandel bestandsnaamconflicten zonder overschrijven.
- [ ] F3.3e — Ondersteun bevestigen en corrigeren van de voorgestelde winkel. Bewaar de beslissing en herkomst.

## F3.4 — Artikels per winkel opbouwen

- [ ] F3.4a — Bewaar artikelwaarnemingen met winkel, ruwe en genormaliseerde omschrijving, eventuele artikelcode, eenheid, OCR-score en verwijzingen naar bronbon en bronregions.
- [ ] F3.4b — Bewaar waarnemingen en schrijfvarianten afzonderlijk. Maak aantallen waarnemingen, exacte matches, kleine afwijkingen en bevestigde koppelingen zichtbaar.
- [ ] F3.4c — Bouw bevestigde winkelartikels en hun toegestane omschrijvingsvarianten op. Onzekere waarnemingen worden niet automatisch bevestigde artikelkennis.
- [ ] F3.4d — Voorkom zelfversterking: een automatisch herstelde OCR-lezing telt niet als onafhankelijk bewijs voor dezelfde correctie. Herimport van dezelfde bon verhoogt de waarnemingsteller niet opnieuw.
- [ ] F3.4e — Bewaar prijzen per aankoopwaarneming. Een prijswijziging maakt niet automatisch een nieuw artikel en een gekende prijs overschrijft geen gelezen aankoopprijs.
- [ ] F3.4f — Ondersteun handmatig koppelen, afwijzen en corrigeren van artikelkandidaten, met behoud van historie en brongegevens.

## F3.5 — Gekende artikels gebruiken bij OCR-onzekerheid

- [ ] F3.5a — Zoek binnen de gekende winkel eerst op expliciete artikelcode en daarna op omschrijving, eenheid en bevestigde varianten.
- [ ] F3.5b — Gebruik OCR-scores en brongeometrie om verdachte lezingen gericht te onderzoeken. Bewaar oorspronkelijke lezing, voorstel, aanwijzingen en alternatieven.
- [ ] F3.5c — Maak onderscheid tussen een betrouwbare match, een reviewvoorstel en geen match. Meerdere plausibele kandidaten blijven onzeker.
- [ ] F3.5d — Leid uit een omschrijvingsmatch geen ontbrekend aantal of aankoopprijs af. Gebruik financiële aansluiting uitsluitend als aanvullende controle.
- [ ] F3.5e — Leg matchregels en acceptatiegrenzen expliciet vast en test gevallen net boven en onder die grenzen.

## F3.6 — Financiële controle en acceptatiepoort

- [ ] F3.6a — Controleer aantal/gewicht × eenheidsprijs, inclusief aantoonbaar gekoppelde korting, tegenover het afzonderlijk gelezen regelbedrag.
- [ ] F3.6b — Controleer artikelbedragen, bonkortingen, retouren en statiegeld tegenover subtotaal en eindtotaal, zonder dubbeltelling.
- [ ] F3.6c — Controleer beschikbare btw-groepen, tarieven, grondslagen en bedragen. Verzin geen ontbrekende btw-verdeling.
- [ ] F3.6d — Houd OCR-onzekerheid, artikelmatchstatus en financiële controles afzonderlijk. Bewaar `match`, `mismatch` en `not_checkable` met reden en verschil.
- [ ] F3.6e — Blokkeer definitieve opslag bij onopgeloste artikelmatches, financiële mismatches of ontbrekende verplichte controles. Een sluitend totaal met compenserende regelfouten wordt niet goedgekeurd.
- [ ] F3.6f — Definieer welke controles verplicht zijn voor goedkeuring. `not_checkable` geldt nooit stilzwijgend als geslaagde controle.
- [ ] F3.6g — Toon vóór bevestiging winkel, artikels, correctievoorstellen, ontbrekende gegevens en regel-/totaal-/btw-controles. Bewaar de expliciete goedkeuring of afwijzing. Een bevestiging omzeilt geen onopgeloste verplichte controle.

## F3.7 — Definitieve opslag en herstartbaarheid

- [ ] F3.7a — Gebruik expliciete verwerkingsstatussen, waaronder `needs_review`, `ready`, `done` en `failed`, met vastgelegde overgangsvoorwaarden.
- [ ] F3.7b — Sla een goedgekeurde bon met alle regels, correcties en btw-groepen in één databasetransactie definitief op. Bij fouten blijft geen gedeeltelijke bon als `done` achter.
- [ ] F3.7c — Bewaar bij definitieve gegevens de bronidentiteit, gebruikte interpretatie en herkomst van de goedkeuring.
- [ ] F3.7d — Maak herstart na onderbreking veilig. Bestaande definitieve aankopen worden niet gedupliceerd of stilzwijgend gewijzigd.
- [ ] F3.7e — Rapporteer verwerkingsfouten, reviewgevallen en definitief opgeslagen bonnen afzonderlijk.

## F3.8 — PDF-afhandeling en onbeheerde runs

- [ ] F3.8a — Behoud de PDF zolang verwerking, review of definitieve opslag niet succesvol afgerond is.
- [ ] F3.8b — Archiveer een PDF pas na geslaagde definitieve databaseopslag. Bewaar de koppeling tussen documenthash, databasegegevens en archiefpad.
- [ ] F3.8c — Handel archieffouten en naamconflicten veilig af. Een volgende run herkent dat de bon al opgeslagen is en kan de archivering hervatten.
- [ ] F3.8d — Voeg onbeheerde verwerking toe met expliciet ingestelde acceptatieregels. Alleen volledig geaccepteerde bonnen mogen automatisch `done` worden; overige blijven voor review beschikbaar.
- [ ] F3.8e — Laat automatisch verwijderen standaard uitgeschakeld. Eventueel verwijderen vereist een afzonderlijk expliciet gekozen beleid en mag pas na succesvolle opslag en vereiste archivering plaatsvinden.

## F3.T — Tests en datasetcontrole

- [ ] F3.T1 — Test databaseopbouw, migraties en export/import van deelbare winkel- en artikelkennis via de SQL-scripts.
- [ ] F3.T2 — Test winkelherkenning met gekende, onbekende en ambigue winkels en met tegenstrijdige foldercontext.
- [ ] F3.T3 — Test exacte artikelmatches, kleine OCR-afwijkingen, meerdere plausibele kandidaten, prijswijzigingen en bescherming tegen zelfversterkende artikelkennis.
- [ ] F3.T4 — Test stuk-/gewichtsartikels, kortingen, retouren, statiegeld, meerdere btw-groepen, ontbrekende gegevens en compenserende regelfouten.
- [ ] F3.T5 — Test MySQL-opslag, transactieterugdraaiing, dubbele import, herstart na onderbreking en verbindings-/schrijffouten.
- [ ] F3.T6 — Test dat onopgeloste reviewgevallen niet in definitieve tabellen als `done` terechtkomen en hun PDF behouden.
- [ ] F3.T7 — Test folderindeling, archieffouten en herhaalde runs zonder dubbele aankopen, overschreven bestanden of verlies van bronbestanden.
- [ ] F3.T8 — Controleer de aangeleverde tickets tegenover onafhankelijk gelezen verwachtingen. Rapporteer per ticket winkel, artikelmatches, gemiste/extra regels, kritieke veldverschillen en acceptatiestatus.
- [ ] F3.T9 — Controleer dat bestaande `pdf2cvs`-, OCR-, interpretatie- en exporttests blijven slagen.

## F3.O — Oplevering

- [ ] F3.O1 — Lever volledige databaseopbouw en migraties onder `DB/`, configuratievoorbeelden zonder geheimen en voorbeeldruns voor begeleide en onbeheerde verwerking.
- [ ] F3.O2 — Rapporteer uitgevoerde tests en datasetresultaten met concrete aantallen en beperkingen. Maak onderscheid tussen geïmplementeerd, automatisch getest en handmatig gecontroleerd.
- [ ] F3.O3 — Demonstreer de volledige keten: winkel herkennen, artikelkennis gebruiken, onzekerheid beoordelen, bevestigen, definitief opslaan en PDF archiveren.
- [ ] F3.O4 — Demonstreer een afgekeurde bon: zichtbaar reviewgeval, geen definitieve opslag als `done` en geen opruiming van de PDF.