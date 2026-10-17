# Instructies voor C/C++-projecten

## 1. Doel en werkwijze

Lever leesbare, onderhoudbare C/C++-code die de gevraagde functionaliteit werkelijk uitvoert. De gebruiker leest C/C++ vlot; elegante code en een duidelijke diff zijn belangrijker dan uitgebreide uitleg van elementaire programmeerconcepten.

Lees vóór implementatie dit bestand, eventuele aanvullende instructies binnen de betrokken directories, `TASK.md`, projectdocumentatie en relevante bestaande code. Bekijk ook verkenningsmateriaal en referentie-implementaties waarnaar de taak verwijst. Begin niet op basis van alleen de titel of een oppervlakkige lezing.

`TASK.md` bepaalt de scope, fasen, volgorde en acceptatiecriteria. Dit bestand bepaalt de vaste werkwijze en codekwaliteit. Expliciete gebruikersinstructies gaan voor.

## 2. Stapsgewijs opleveren via TASK.md

- Werk per afgebakende stap of fase uit `TASK.md`. Respecteer expliciete stop- en oplevermomenten; voer niet zonder opdracht alle volgende fasen uit.
- Rond binnen de opgedragen stap implementatie, build, relevante tests en review af vóór oplevering.
- Elke taak- of faseoplevering moet aan jouw kant een daadwerkelijk uitgevoerde, groene CTest-run hebben na de laatste codewijziging. Zonder die groene run is de taak niet voltooid; meld dan uitsluitend een onvolledige/geblokkeerde status met de concrete oorzaak.
- Elke stap levert samenhangende, beoordeelbare functionaliteit. Een lege interface, stub, hardcoded demonstratie of CLI die de echte verwerking niet aanroept, telt niet als voltooide functionaliteit.
- Verbind nieuwe onderdelen met het werkelijke uitvoeringspad. Laat geen ongebruikte objecten, dode code, dubbele implementaties of onafgewerkte tijdelijke constructies achter.
- Vink een onderdeel pas af wanneer aan de acceptatiecriteria is voldaan. Pas criteria niet aan om een onvolledige oplossing toch als voltooid te presenteren.
- Meld een concrete blokkade met oorzaak en gevolgen. Maak helder wat wel werkt en wat nog ontbreekt.
- Houd wijzigingen gericht op de huidige stap. Vermijd ongevraagde herstructurering, massale formattering of vooruitlopende architectuur.

Ontbreekt een bruikbare fasering voor een grote taak, beschrijf dan eerst kleine, controleerbare stappen. Gebruik de bestaande opdracht om de eerste stap uit te voeren; verzin geen aanvullende toestemmingsprocedure voor routinewerk.

## 3. Repository, sandbox en externe libraries

Je werkt in een clone/sandbox van de repository. Dit is geen garantie dat de complete ontwikkelomgeving, alle dependencies, credentials, services of platform-SDK's beschikbaar zijn. Controleer wat aanwezig is en volg de bestaande buildinstructies.

### extern/ is alleen-lezen

Een directory `extern` in de repository-root vertegenwoordigt externe repositories/libraries en moet behandeld worden als git-submodulegebied, ook wanneer de sandbox de inhoud als gewone directories toont.

- Lees de code en publieke API om bestaande functionaliteit te begrijpen en te hergebruiken.
- Wijzig, verwijder, herformatteer of genereer geen bestanden onder `extern/`.
- Wijzig geen submodulecommit, gitlink, remote of `.gitmodules` om een probleem te omzeilen.
- Plaats geen buildoutput of tijdelijke patches onder `extern/`; gebruik een afzonderlijke builddirectory.
- Het initialiseren/ophalen van ontbrekende, reeds geregistreerde submodules via de bestaande CMake-bootstrap is toegestaan: daarmee wordt de vastgelegde dependency beschikbaar gemaakt. Dit geeft geen toestemming om de opgehaalde broncode, submodulecommit of registratie te wijzigen. Behoud lokale wijzigingen en haal geen andere revisie op dan het project voorschrijft.
- Een noodzakelijke verbetering in een externe library wordt beschreven in `suggestie.md` in de root van het hoofdproject, buiten `extern/`.
- Vermeld daarin library/pad, probleem, voorgestelde API of wijziging, onderbouwing, gevolgen voor het hoofdproject en passende tests. Maak duidelijk of de huidige taak hierdoor geblokkeerd is.

Goede herbruikbare functionaliteit hoort in de verantwoordelijke library. Kopieer geen librarycode naar elk afgeleid project en bouw geen parallelle implementatie om de alleen-lezenregel te omzeilen. Een projectspecifieke adapter is passend wanneer die werkelijk projectspecifiek gedrag toevoegt en de bestaande library-API gebruikt.

## 4. Architectuur en eigenaarschap van de API

Volg de bestaande taalstandaard, modules, dependencies en verantwoordelijkheden. Introduceer geen framework, dependency of taalstandaardwijziging zonder noodzaak binnen de opdracht.

Voor C++: geef gedrag een duidelijke eigenaar. Een bewerking die bij één class of type hoort, wordt een memberfunctie of `static` memberfunctie. Dit geldt ook voor creatie, conversie, mapping en validatie.

Gebruik publieke vrije functies alleen wanneer er geen natuurlijke eigenaar bestaat, de functie bewust generiek is voor meerdere ongerelateerde types, of een C-API vereist is. Beoordeel bestaande vrije functies bij wijzigingen, maar verplaats ze niet buiten de opgedragen scope zonder noodzaak.

Voor C: behoud een consistente C-API met passende modulegrenzen; forceer geen C++-constructies in C-code.

### Helpers en namespaces

- Kleine helperlogica die één keer wordt gebruikt, kan een lokale lambda zijn.
- Helperlogica die meerdere members gebruiken, wordt een private memberfunctie.
- Herbruikbare functionaliteit krijgt een eigen type of module in de juiste library.
- Verzamel geen ongerelateerde helpers in een anonymous namespace.
- Omhul een implementatiebestand niet standaard met een anonymous namespace. Gebruik die uitsluitend wanneer er een concrete technische noodzaak is.

## 5. Naamgeving

### Rijke namespaces, eenvoudige typenamen

Geef de voorkeur aan een betekenisvolle namespacehiërarchie en korte, duidelijke objectnamen boven lange class-/struct-/enumnamen die de volledige modulecontext herhalen. De namespace en subfolder dragen de context; de typenaam benoemt het concept.

Bijvoorbeeld: verkies `MyProject::Market::History::Request` boven `MyProject::MarketHistoryRequest`. Voeg alleen context aan de typenaam toe wanneer die binnen de eigen namespace nodig is om het concept ondubbelzinnig te maken. Gebruik de bestaande defaultnamespace van het project; verzin geen afwijkende projectnamespace.

### Namespaces en subfolders tijdig indelen

- Deel samenhangende concepten tijdig in onder passende namespaces en subfolders. Wacht niet tot één directory of namespace een verzameling van ongerelateerde objecten is geworden.
- Laat namespace-, folder- en module-indeling dezelfde verantwoordelijkheden zichtbaar maken. Behoud de bestaande scheiding tussen publieke headers, private headers en implementaties.
- Maak geen diepe hiërarchie zonder inhoudelijke betekenis. Iedere namespace/subfolder moet een herkenbaar domein of verantwoordelijkheid vertegenwoordigen.
- Neem bij het toevoegen van een namespace of subfolder de bestaande types, bestanden en hun gebruikers binnen de betrokken module en aangrenzende verantwoordelijkheden door. Beoordeel of de gekozen naam al bestaat, of een bestaande locatie beter past, en welke bestaande concepten bij de nieuwe indeling horen.
- Laat geen dubbele conceptgroepen of oude plaatsingen achter uitsluitend omdat alleen het nieuwe bestand is bekeken. Voer samenhangende verplaatsingen/renames binnen de huidige taak uit wanneer die nodig zijn voor een consistente indeling. Leg voorstellen buiten de scope vast voor een volgende fase.

### Interne API over meerdere implementatiebestanden

Een interne API die door meerdere `.cpp`-bestanden wordt gebruikt, hoort onder:

```cpp
<project_default_namespace>::Internal::<optionele_subnamespace>
```

Bijvoorbeeld `MyProject::Internal::Parsing::Token`. Plaats de bijbehorende headers in de private source-/internal-indeling van de module, niet in de publieke include-API. Een namespace met `Internal` in de naam is op zichzelf geen reden om private headers publiek te exporteren.

Deze regel geldt niet voor lokale helpers in één `.cpp` die niet door andere `.cpp`-bestanden worden gebruikt. Pas daarvoor de bestaande helperregels toe; maak geen gedeelde interne API of extra namespace uitsluitend voor lokale implementatiedetails.

### Verplaatsingen en renames vastleggen

Beschrijf structurele refactors, namespace-/folderverplaatsingen en renames kort in de oplevering of commitbeschrijving: oude en nieuwe naam, reden, gevolgen voor includes/targets/callers en uitgevoerde build-/CTest-validatie. Maak hiervoor geen apart Markdown-logboek.

Een rename is pas afgerond wanneer declaraties, definities, includes, callers, tests en CMake-verwijzingen consistent zijn bijgewerkt. `extern/` blijft alleen-lezen.

### Prefixes voor opslag en eigenaarschap

Gebruik prefixes om opslag/eigenaarschap zichtbaar te maken, niet om types te coderen.

| Soort | Conventie | Voorbeeld |
| --- | --- | --- |
| Niet-publiek, niet-static classveld | `m_` | `m_document`, `m_scrollOffset` |
| Static object/veld | `s_` | `s_instance` |
| Globaal object | `g_` | `g_application` |
| Lokale variabele | Geen prefix | `document`, `offset` |
| Functieparameter | Geen prefix | `document`, `offset` |
| Publiek structveld/plain data | Geen prefix | `width`, `height` |

Gebruik geen trailing underscore voor membervelden, zoals `document_`. Houd overige type- en functienamen consistent met het project. Vermijd nieuwe globale objecten wanneer eigenaarschap binnen een bestaand type past.

## 6. Classindeling, headers en implementatie

### Types verdelen over headers

Stop niet te veel concepten in één header. Een header met meer dan drie eigen `struct`-, `class`- en `enum`-definities moet worden opgesplitst in samenhangende headers; tel deze definities gezamenlijk, niet per soort. De grens van drie is een bovengrens, geen doel: splits eerder wanneer types verschillende verantwoordelijkheden hebben.

Geef zelfstandige concepten een eigen header op de passende namespace-/folderlocatie. Behoud kleine, sterk samenhangende typegroepen wanneer die gezamenlijk hoogstens drie definities bevatten. Gebruik geen nesting, verplaatsing naar een grote `.cpp` of forward declarations om de vereiste conceptuele indeling te omzeilen. Noodzakelijke forward declarations tellen niet als type-definities.

Controleer de verdeling bij het toevoegen van ieder nieuw type en bij iedere faseoplevering. Pas includes en relevante build-/testverwijzingen aan en beschrijf structurele splitsingen in de oplevering. Pas deze regels toe op eigen projectcode; externe headers onder `extern/` blijven ongemoeid.

### Velden vóór constructors en functies

Declareer de volledige objectstaat bovenaan de class, direct na de eerste access specifier. Plaats velden altijd vóór constructors, destructor, publieke/protected functies en private helpers. Behoud de juiste toegankelijkheid; velden hoeven hiervoor niet publiek te worden.

```cpp
class DocumentStore
{
private:
    std::string m_path;
    bool m_loaded = false;

public:
    explicit DocumentStore(std::string path);
    bool load();
    void clear();

private:
    bool validate() const;
};
```

### Header en source blijven gesynchroniseerd

De header bepaalt de architectuur; de source implementeert die. Houd de volgorde van implementaties in de `.cpp` exact gelijk aan de declaratievolgorde in de header, voor de functies die daar worden geïmplementeerd.

### Korte logica in de header, zware bodies in de source

Laat korte, overzichtelijke functie-/memberbodies bij voorkeur direct bij hun declaratie in de header staan wanneer aan beide grenzen wordt voldaan:

- Minder dan vijf statements: maximaal vier.
- Minder dan tien regels voor de volledige definitie, inclusief signature, accolades en witruimte: maximaal negen.

Houd de logica leesbaar en pas de gewone opmaakregels toe, inclusief de lege regel na `return`/`break`. Pers geen statements of regels samen om onder de grenzen te blijven. Een constructor met een eenvoudige initializer list kan eveneens in de header staan wanneer de volledige definitie binnen deze grenzen blijft.

Verplaats grotere/zwaardere bodies naar de `.cpp`. Plaats ook een korte body in de `.cpp` wanneer die een private interne struct, class of andere implementatiedetails nodig heeft die niet in de publieke API/header horen. Maak interne types of includes niet publiek uitsluitend om de functie inline te kunnen definiëren. Gedeelde interne types volgen de eerder beschreven `Internal`-namespace-/private-headerregels; uitsluitend lokale types blijven lokale implementatiedetails.

De header blijft in beide gevallen het overzicht van de class en haar lichte logica. Sla bij het bepalen van de `.cpp`-volgorde de al in de header gedefinieerde functies over; de overige bodies behouden exact de relatieve declaratievolgorde uit de header. Definieer een functie niet nogmaals in de `.cpp` als de body al in de header staat. Behoud technisch noodzakelijke headerdefinities, bijvoorbeeld voor templates, en voldoe aan de ODR-regels bij vrije headerfuncties.

Groepeer verwante declaraties en korte inline definities zonder onnodige lege regels. Voeg een lege regel toe wanneer het onderwerp verandert of een lange declaratie/definitie een zelfstandig visueel blok vormt.

Groepeer constructors/destructor, initialisatie, publieke API, rendering, input, layout, serialisatie en interne helpers waar van toepassing. Houd deze groepering gelijk in header en source.

Gebruik witruimte voor kleine groepen. In grotere bestanden, ongeveer boven 150 regels, kunnen korte sectiesamenvattingen helpen. Wanneer het project regions ondersteunt/gebruikt, heeft één brede region voor samenhangende groepen de voorkeur boven vele kleine regions. Introduceer geen compilerafhankelijke regions die de ondersteunde toolchains verstoren.

## 7. Leesbare code en witruimte

- Geen code-dumps: plaats geen complete functies, classes of meerdere onafhankelijke statements op één regel.
- Gebruik normale inspringing en overzichtelijke regels. Breek lange signatures, expressies en calls op logische grenzen.
- Houd samenhangende code bij elkaar en scheid verschillende onderwerpen met één lege regel.
- Voeg korte comments toe wanneer die intentie, een beperking of een niet-vanzelfsprekende keuze verklaren. Beschrijf niet wat de code al duidelijk zegt.
- Volg voor overige opmaak de bestaande projectstijl, zolang die niet met deze regels botst.

### Bodies met één statement

Laat accolades weg bij `if`, `else`, `for` en `while` met precies één statement. Zet de body op een eigen, ingesprongen regel.

Gebruik accolades wanneer meerdere statements nodig zijn, een expliciete scope nodig is, of weglaten werkelijk ambiguïteit veroorzaakt, bijvoorbeeld bij een dangling `else`. Voeg geen accolades toe uitsluitend vanwege een formatterdefault.

### Witruimte na return en break

Plaats na een `return`- of `break`-statement altijd een lege regel, ook vóór een sluitende accolade. Alleen een kleine, overzichtelijke lambda mag hiervan afwijken.

```cpp
bool DocumentStore::load()
{
    if (m_path.empty())
        return false;

    for (const auto& entry : entries)
        process(entry);

    switch (state)
    {
    case State::Ready:
        activate();
        break;

    default:
        return false;

    }

    return true;

}
```

Een kleine lambda zoals `[](const auto& item) { return item.valid(); }` is toegestaan. Dit is geen vrijbrief voor lange lambdas of functies op één regel.

## Projectafspraken ReceiptLedger

- De OCR-engine staat onder `engine/ocr`, met namespace `ReceiptLedger::Engine::Ocr` en targetalias `ReceiptLedger::engine_ocr`. Behoud deze gereviewde indeling. Nieuwe boninterpretatie krijgt een passende eigen engine-module; de OCR-engine blijft verantwoordelijk voor herkenning.
- De appmodule en CMake-target zijn `app/pdf2csv` en `ReceiptLedger_app_pdf2csv`, met namespace `ReceiptLedger::App::PDF2CSV`. Houd directorycasing ook in Git exact gelijk. De CLI, orchestration en exports blijven in deze app tijdens fase 2; geen ongevraagde appopsplitsing. De uitvoernaam is `pdf2cvs.exe` op Windows en `pdf2cvs` op andere platformen.
- Dependencies worden via CppDependencies beheerd. Gebruik in Codex de PDFium-binaryroute om de zware sourcebuild te vermijden:

```sh
cmake -S . -B build-codex -DCPPDEPENDENCIES_PDFIUM_USE_BINARIES=ON
cmake --build build-codex --config Release
ctest --test-dir build-codex -C Release --output-on-failure
```

Deze flag is een CMake-optie, geen compilerflag. De dependency behoudt haar bestaande standaard voor andere builds.

- `TASK.md` bevat alleen het resterende fase-2-werk en de acceptatiecriteria. Geen historische logs of nieuwe ontwerp-/review-Markdownbestanden; beschrijf validatie in de oplevering. De door de app gegenereerde `ocr-regions.md` blijft herkenningsoutput.

## 8. Build, CMake, CppCMake en dependencies

### Vaste projectbasis

CMake is het standaardbuildsysteem. Modules zoals apps en libraries worden via CMake en CppCMake opgebouwd, zodat directorystructuur, targets, dependencies en `CMakeLists.txt` consistent en gemakkelijk te inspecteren blijven.

De vaste bouwstenen van de gebruiker zijn:

- [CppCMake](https://github.com/Iso83/CppCMake): projectinitialisatie, moduletemplates en -registratie, target-/IDE-indeling, submodule-bootstrap, versieheaders, tests en runtime-deployment.
- [CppDependencies](https://github.com/Iso83/CppDependencies): herbruikbare dependencyresolvers, door de gebruiker ook `cppdeps` genoemd; de concrete API gebruikt `cppdependencies_*`.
- [CppProjectSkeleton](https://github.com/Iso83/CppProjectSkeleton): de template waarmee de gebruiker nieuwe projecten aanlevert. De aangeleverde template gebruikt CMake 4.0 als minimum en C++20.

Lees bij aanvang de root- en module-`CMakeLists.txt`, relevante presets en de documentatie/implementatie van de in het project gebruikte CppCMake- en cppdeps-functies. De meegeleverde versie is leidend; verzin geen functienamen, argumenten of gedrag op basis van de repositorynaam. Als de online documentatie niet bereikbaar is, gebruik dan de lokaal beschikbare bestanden en meld ontbrekende informatie gericht.

### Modules en dependencybeheer

- Behoud de opbouw van CppProjectSkeleton bij nieuwe projecten die op deze template zijn gebaseerd.
- De root-`CMakeLists.txt` verzorgt bootstrap, projectinitialisatie, dependencies en moduleregistratie. De module-`CMakeLists.txt` beschrijft de eigen sources, publieke/private includes, target, links en tests. Vermijd één monolithisch roottarget waarin alle apps en libraries worden samengevoegd.
- Registreer modules via `cppcmake_module_add(<relatief/pad>)`, bijvoorbeeld `engine/core` of `apps/editor`. Deze functie genereert ontbrekende modules via `cppcmake_module_deploy()`, registreert ze eenmaal per project en voert `add_subdirectory()` uit. Bestaande moduledirectories worden niet opnieuw gegenereerd.
- Gebruik de gegenereerde indeling: `include/<Project>/<modulepad>/` voor publieke headers, `src/` voor implementaties en private headers, `tests/` voor tests, en een eigen `CMakeLists.txt` per module.
- Behoud de targetconventie `<Project>_<modulepad_met_underscores>` en, voor libraries, de alias `<Project>::<modulepad_met_underscores>`. Gebruik de aanwezige versie-/attributehelpers, IDE-folders en `source_group()`-indeling zodat modules en hun publieke/private bestanden gemakkelijk te inspecteren zijn.
- De huidige moduletemplate maakt een library met `add_library()`, ook als het pad onder `apps/` staat. Maak voor een app bewust de passende executable-indeling en de hieronder beschreven INTERFACE-library voor testbare app-logica; neem niet aan dat de padnaam automatisch het targettype kiest.
- Respecteer de source-discovery van de aanwezige template (`GLOB_RECURSE` met `CONFIGURE_DEPENDS`) in plaats van die zonder aanleiding te vervangen door een andere aanpak. Controleer dat nieuwe bestanden binnen de juiste module worden opgenomen.
- Gebruik bestaande CppCMake-bouwstenen voor de verantwoordelijkheden die ze al afdekken. Dupliceer die logica niet in zelfgeschreven CMake of losse buildscripts.
- Houd de modulegrenzen zichtbaar in de directorystructuur en de bijbehorende `CMakeLists.txt`; maak duidelijk welke target bij welke module hoort en welke dependencies die target gebruikt.
- Beheer dependencies via de bestaande `cppdeps`-integratie waar die is ingericht. Introduceer geen parallelle dependencyroute, handmatige downloads of hardcoded lokale include-/librarypaden om die integratie te omzeilen.
- Leg targetdependencies en usage requirements vast bij de verantwoordelijke target, met passende `PRIVATE`, `PUBLIC` of `INTERFACE`-zichtbaarheid volgens de bestaande projectconventies.
- Houd build, tests en module-indeling onder dezelfde CMake-opbouw. Een losse compileropdracht of script dat alleen buiten deze opbouw werkt, is geen voltooide integratie.
- Blijven CppCMake of CppDependencies onder `extern/`, dan geldt ook daarvoor de alleen-lezenregel. Beschrijf noodzakelijke verbeteringen in `suggestie.md`.

### Een GitHub-clone moet dependencies zelf kunnen verkrijgen

De gebruiker wil dat iemand die het project van GitHub clonet geen ontbrekende projectlibraries handmatig hoeft te zoeken, downloaden, compileren of aan lokale paden koppelen. Een geschikte toolchain, Git, CMake, netwerktoegang en gedocumenteerde platform-/systeempakketten mogen vereist zijn. Toegang tot de persoonlijke NAS of BuildFarm mag geen voorwaarde zijn.

Behoud deze dependencyroute:

1. Hergebruik een geschikte target die al door het hoofdproject of een andere module beschikbaar is gesteld. Maak geen dubbele targets of builds van dezelfde library.
2. Gebruik beschikbare compatibele packages, waaronder vooraf gebouwde BuildFarm-packages op de NAS, via de bestaande resolver/package-integratie.
3. Als geen bruikbare package beschikbaar is, haal de benodigde sources automatisch op via de resolver en CMake `FetchContent` en bouw ze in de normale projectbuild. Dat kan een Git-fetch of een versiearchief zijn; de resolver bepaalt de route. Waar bronnen al worden meegeleverd, zoals bij GLAD en SQLite, gebruikt de resolver die.

Dit is de gewenste werkwijze; niet iedere resolver heeft dezelfde tussenstappen. Lees de daadwerkelijk gebruikte wrapper. Sommige wrappers gaan na een bestaande target rechtstreeks naar FetchContent. Presenteer het gewenste eindresultaat niet als bewijs dat elke bestaande resolver of elke platformcombinatie al werkt.

- Laad de optionele machineconfiguratie vóór dependencyresolutie met `cppcmake_dependency_try_local_packages()`. `CPPCMAKE_DEPENDENCY_LOCAL_PACKAGE_FILE` kan packagepaden/helpers beschikbaar maken. Een ontbrekend of niet-ingesteld configuratiebestand mag de source-route niet uitschakelen. Hardcode geen NAS-pad in de gedeelde projectconfiguratie.
- Behoud de bootstrap van CppCMake via `dependencies_fetch_cppcmake()` en de bestaande initialisatie van geregistreerde submodules via `cppcmake_gitsubmodule_init_missing_targets()`. Gebruik `cppcmake_dependency_add_subdirectory_unless_target()` voor externe projectlibraries die alleen toegevoegd moeten worden als hun target ontbreekt.
- Include `extern/CppDependencies/CppDependencies.cmake` op de bestaande integratieplek en vraag alleen de werkelijk gebruikte libraries op met hun `cppdependencies_*`-resolver. Het includen van het bestand activeert niet automatisch alle dependencies. 'Alles bouwen' betekent de volledige benodigde dependencyketen van de gekozen projecttargets, niet de hele catalogus.
- Link de door de resolver teruggegeven target(s), bijvoorbeeld na `cppdependencies_opencv(OPENCV_TARGETS STATIC COMPONENTS core imgproc)`. Respecteer vereiste linkagekeuzes, componenten, versies en targetnamen. Gebruik geen losse `.lib`-paden als vervanging voor targets.
- Behoud de vastgelegde versies, commits en archivehashes. Vervang ze niet door willekeurige nieuwste versies om een configurefout te omzeilen. Gebruik de in het project vastgelegde CppCMake-bootstraprevisie.
- Gebruik `cppcmake_dependency_make_available()` voor bestaande FetchContent-integratie en de dependency-folderhelpers voor overzichtelijke `third_party`-targets.
- Houd geneste projecten composable: hergebruik bestaande targets en de actieve CppDependencies-provider; forceer geen tweede provider zonder concrete noodzaak.
- Deploy benodigde Windows-DLL's via `cppcmake_dependency_copy_runtime()` na het linken van de app-target. De testhelper regelt dit zelf voor testexecutables. Runtimebestanden blijven buildoutput en horen niet in de PR-diff.
- Is een benodigde resolver afwezig of werkt de fallback niet, lever dan een concrete `suggestie.md` voor CppDependencies/CppCMake. Kopieer de gedeelde resolverlogica niet naar elk afgeleid project. Meld een daardoor geblokkeerde taak eerlijk.

### Tests blijven onderdeel van de moduleopbouw

Gebruik `cppcmake_test_setup()` en `cppcmake_test_add(NAME ... TARGET ... SOURCES ...)` volgens het project. Controleer zowel `<Project>_BUILD_TESTS` als `BUILD_TESTING`; de helper registreert geen tests als die uitstaan. Tests van het hoofdproject staan in de huidige opzet standaard aan, van geneste projecten standaard uit.

### Apps testen via een INTERFACE-library

Gebruik voor app-logica een `add_library(... INTERFACE)`-target met de gedeelde sources, headers, include requirements, compile requirements en dependencies. Zowel de app-executable als de testexecutables linken deze interface. De gedeelde app-sources worden zo in beide executables gecompileerd; er is geen aparte STATIC-supportlibrary nodig uitsluitend om de app testbaar te maken.

- Houd `Main.cpp` uitsluitend in de app-executable. Het mag nooit onderdeel zijn van de INTERFACE-sources of testtargets.
- Geef iedere testexecutable zijn eigen test-entrypoint. Link tests niet aan de app-executable en neem geen tweede `main()` op.
- Gebruik `target_sources(... INTERFACE ...)` met ondubbelzinnige sourcepaden en passende `target_include_directories()`, `target_compile_features()` en `target_link_libraries()` op de interface. Voorkom dat dezelfde sources daarnaast nog afzonderlijk aan de executable worden toegevoegd.
- Toets via deze interface de echte app-logica: bijvoorbeeld verwerking, state transitions, argumentinterpretatie en orchestration. Het starten van de app of controleren van alleen `--help` vervangt deze tests niet.
- Behoud al bestaande echte libraries als libraries; de INTERFACE-aanpak geldt voor het delen van app-logica met apptests, niet als algemene omzetting van alle librarytargets.

### Benut de parameters van cppcmake_test_add

`cppcmake_test_add()` is een uitgebreide helper; bouw niet zelf een parallel testtarget omdat je alleen de minimale aanroep kent. De beschikbare parameters in de aangeleverde versie zijn:

| Parameter | Gebruik |
| --- | --- |
| `NAME` | Verplicht: naam van de testexecutable én de CTest-registratie. |
| `TARGET` | Verplicht: de te testen librarytarget; voor app-logica de INTERFACE-library. |
| `SOURCES` | Verplicht: één of meer testsources, inclusief het test-entrypoint. |
| `MODULE` | Optioneel: modulepad voor de IDE-indeling onder tests. |
| `LABELS` | Optioneel: CTest-labels voor gerichte selectie en groepering. |
| `LINK_LIBRARIES` | Optioneel: aanvullende dependencies die alleen de test nodig heeft. |
| `INCLUDE_DIRECTORIES` | Optioneel: aanvullende testincludes, bijvoorbeeld private moduleheaders uit `src/`. |
| `COMPILE_DEFINITIONS` | Optioneel: gerichte compile definitions voor de testtarget. |

Gebruik deze parameters waar passend. De helper maakt de executable, linkt `TARGET` en aanvullende libraries, voegt testhelperincludes toe, registreert de test bij CTest, stelt de IDE-folder/labels in en deployt runtime-DLL's. Extra noodzakelijke CTest-properties kunnen daarna gericht met `set_tests_properties()` worden ingesteld; dit is geen reden om de helper te omzeilen.

Een app-indeling kan er als volgt uitzien; pas namen en requirements aan de module aan:

```cmake
add_library(MyProject_apps_tool_support INTERFACE)

target_sources(MyProject_apps_tool_support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Application.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Application.h"
)
target_include_directories(MyProject_apps_tool_support INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_compile_features(MyProject_apps_tool_support INTERFACE cxx_std_20)

add_executable(MyProject_apps_tool src/Main.cpp)
target_link_libraries(MyProject_apps_tool PRIVATE MyProject_apps_tool_support)
cppcmake_dependency_copy_runtime(MyProject_apps_tool)

cppcmake_test_add(
    NAME Test_MyProject_apps_tool
    TARGET MyProject_apps_tool_support
    MODULE apps/tool
    SOURCES tests/Test_Application.cpp
    LABELS app regression
)
```

De gegenereerde `CPPTEST_ASSERT(true)` is uitsluitend een placeholder. Vervang die door inhoudelijke API-/regressietests voordat de module als getest wordt opgeleverd. De aanwezige `TestAssert.h` met `CPPTEST_ASSERT` en `CPPTEST_RUN` houdt checks actief onafhankelijk van `NDEBUG` en geeft fouten door als een niet-nul resultaat. Een aanvullend testframework is niet nodig uitsluitend om zinvolle checks te schrijven.

### Buildcontrole

- Gebruik de bestaande CMake-configuratie, presets, targets en testinfrastructuur.
- Bouw buiten de source directories. Houd gegenereerde bestanden en dependency-buildoutput uit de brondiff.
- Controleer na wijzigingen dat de betrokken targets werkelijk gebouwd worden; een geslaagde configure is geen geslaagde build.
- Zorg dat nieuwe sources en tests in de juiste targets zijn opgenomen. Controleer ook dat nieuwe productiecode daadwerkelijk aangeroepen wordt.
- Voeg geen tweede executable-entrypoint aan een testtarget toe. Deel app-logica via de hierboven beschreven INTERFACE-library en gebruik `cppcmake_test_add()` met die interface als `TARGET`; houd `Main.cpp` uitsluitend bij de app.
- Verzwak geen compilerinstellingen, tests of dependencycontroles om een groen resultaat te verkrijgen.
- Controleer bij veranderingen aan dependencies of bootstrap ook de route zonder NAS/lokale packageconfiguratie in een verse builddirectory. Een bestaande cache of reeds aangemaakte target bewijst niet dat de fallback werkt. Bouw de gevraagde targets met hun benodigde dependencyketen en voer relevante CTest-tests uit. Rapporteer expliciet als netwerk-, toolchain- of systeempakketbeperkingen deze controle verhinderen; claim dan geen bewezen clean-clone-build.
- Documenteer reproduceerbare clone/configure/build/testcommando's en noodzakelijke toolchain-/systeempakketten in de projectdocumentatie. Verwijs niet naar handmatig te zoeken projectlibraries als standaard installatieprocedure.

## 9. CTest: betekenisvolle toetsing van gedrag

CTest is de runner. De waarde zit in tests die vaststellen dat essentiële API-contracten en functionaliteit correct blijven werken.

### Wat tests moeten controleren

- Roep de echte productie-API aan en controleer inhoudelijke resultaten, toestand en relevante neveneffecten.
- Toets publieke API-contracten, relevante grensgevallen en foutafhandeling.
- Voeg bij een bugfix een regressietest toe die het oorspronkelijke probleem zichtbaar maakt, waar technisch mogelijk.
- Toets belangrijke interne/private logica wanneer dat nodig is. Geef de voorkeur aan observeerbaar gedrag via de bestaande API; gebruik anders een gerichte interne testmogelijkheid zonder uitsluitend voor tests productieleden publiek te maken.
- Gebruik onafhankelijk bepaalde verwachte resultaten. Kopieer de productie-implementatie niet naar de test als berekening van de verwachte uitkomst.
- Houd tests deterministisch en geïsoleerd. Gebruik tijdelijke locaties voor schrijfwerk en passende fixtures; vermijd afhankelijkheid van live diensten of persoonlijke machinepaden tenzij de taak expliciet integratietests vereist.

### Wat niet volstaat

Een executable die alleen start, `--help` afdrukt of exitcode 0 teruggeeft, bewijst geen correcte API. Zulke smoke tests mogen aanvullende waarde hebben, maar vervangen geen inhoudelijke tests. Vermijd tautologische assertions, tests van uitsluitend mocks en tests die slechts bevestigen dat constructors bestaan.

Testchecks moeten ook in Release actief blijven. Gebruik geen kale `assert` als enige verificatie wanneer `NDEBUG` die uitschakelt. Een mislukte check moet een niet-nul exitcode en een bruikbare foutmelding opleveren.

### Verplicht uitvoeren en rapporteren

Een groene CTest-run is een harde oplevervoorwaarde voor iedere taak of fase. Een eerdere groene run, alleen geslaagde compilatie of de verwachting dat tests zullen slagen volstaat niet. Voer de controles na de laatste codewijziging uit in de configuratie die je oplevert.

1. Configureer en bouw de betrokken productie- en testtargets.
2. Controleer met CTest-testdiscovery dat de bedoelde tests geregistreerd zijn; nul gevonden tests is geen succesvolle validatie.
3. Voer de relevante CTest-tests werkelijk uit met `--output-on-failure`. Gebruik bij een multi-config generator de juiste `-C <config>` en volg aanwezige testpresets.
4. Voer de door `TASK.md` of het project verplichte suite uit. Voer daarnaast relevante regressietests uit wanneer gedeeld gedrag is gewijzigd.
5. Los veroorzaakte fouten op en herhaal de betrokken checks.

De opleverrun moet de relevante gedragstests en alle door het project of `TASK.md` verplichte tests omvatten, met nul falende tests. Filter geen falende tests weg, schakel ze niet uit en verzwak hun checks om groen te verkrijgen. Nul gevonden tests, uitsluitend placeholders of overgeslagen verplichte tests voldoen niet. Ook een bestaande testfout mag niet als groen worden gepresenteerd: meld de fout en de onvolledige status; wijzigingen buiten de toegestane scope blijven begrensd door de opdracht.

Kun je door een ontbrekende dependency, netwerkbeperking, toolchain of platform de vereiste CTest-run niet uitvoeren, rapporteer dan wat geprobeerd is en waarom de taak niet aan de oplevervoorwaarde voldoet. Vink de taak niet af en presenteer deze niet als afgerond. Voeg voor nieuwe of gewijzigde functionaliteit ontbrekende inhoudelijke tests toe volgens de bestaande testopbouw.

Rapporteer gebruikte configure-, build- en testcommando's, configuratie en resultaten. Onderscheid geslaagde, mislukte, overgeslagen en niet-uitgevoerde tests. Noem een ontbrekende dependency of unsupported platform concreet. Claim nooit dat tests geslaagd zijn wanneer ze alleen zijn geschreven, bekeken of niet uitgevoerd.

## 10. Binaries en een bruikbare Codex Cloud PR

Binarybestanden zijn op zichzelf toegestaan in het project, bijvoorbeeld bestaande assets of bestanden die de gebruiker aanlevert. De beperking in deze Codex Cloud-workflow betreft binaries die Codex zelf maakt of wijzigt: de gebruiker kan die via deze opleverroute niet binnenhalen en ze kunnen de PR-aanmaak blokkeren. Neem daarom geen door Codex gemaakte of gewijzigde binaries op in de op te leveren PR-diff. Dit geldt onder andere voor executables, libraries, objectfiles, afbeeldingen, animaties, screenshots, PDF's, archives en gegenereerde databases.

- Bouw en test wel: tijdelijke binaries zijn toegestaan in afzonderlijke, genegeerde build-/tempdirectories.
- Commit of stage geen door Codex gemaakte of gewijzigde `.exe`, `.dll`, `.lib`, `.a`, `.so`, `.o`, `.obj`, `.png`, `.jpg`, `.gif`, `.pdf`, `.zip` of andere binaire output. Behandel bestaande of door de gebruiker aangeleverde binaries niet als verboden bestanden; behoud die en neem ze alleen op wanneer de opdracht dat vraagt.
- Plaats screenshots en visuele testoutput buiten de te leveren diff. Voeg geen afbeelding toe aan de repository alleen om voortgang te tonen.
- Lever bevindingen en validatie als tekst aan. Gebruik waar mogelijk tekstfixtures; genereer noodzakelijke binaire testinput tijdens tests in een tijdelijke directory.
- Laat bestaande tracked binaries ongemoeid. Ontstaat toch een binaire wijziging, herstel uitsluitend de eigen onbedoelde wijziging en behoud gebruikerswerk.
- Is een door Codex te maken of wijzigen binary essentieel voor de opdracht, meld het conflict met deze workflow vóór opname in de diff en vraag gericht om een alternatieve leveringsroute. Een door de gebruiker aangeleverde binary valt niet onder dit generatieverbod.

Controleer vóór oplevering zowel staged als unstaged wijzigingen en untracked bestanden, bijvoorbeeld met `git status --short`, `git diff --stat`, `git diff --numstat` en de overeenkomstige `--cached` varianten. Bekijk untracked bestanden afzonderlijk: ze verschijnen niet in `git diff`. Een `-` bij de tellingen in `--numstat` duidt op een door Git als binair behandelde wijziging; controleer ook bestandstype en inhoud.

Breid `.gitignore` alleen gericht uit voor werkelijk gegenereerde bestanden. Een ignore-regel verwijdert geen al tracked binary uit de diff. Gebruik geen blind `git add .` en geen destructieve repository-cleanup om de diff schoon te krijgen.

## 11. Bekende problemen en oplevering

Verberg bekende bugs niet in `TODO`, `FIXME` of `HACK`. Los problemen binnen de huidige scope op. Meld resterende problemen buiten de scope expliciet in de oplevering, met impact en eventuele vervolgstap.

Controleer vóór afronding:

- De opgedragen stap werkt via het echte uitvoeringspad en voldoet aan `TASK.md`.
- De code volgt naamgeving, classindeling, header/source-volgorde en witruimteregels.
- Headers bevatten hoogstens drie eigen type-definities en concepten staan in passende namespaces/subfolders. Gedeelde interne API gebruikt `<project_default_namespace>::Internal::<optioneel>`. Structurele refactors en renames zijn beschreven in de oplevering.
- Build is geslaagd en de CTest-opleverrun na de laatste codewijziging is groen, met nul falende tests en alle verplichte tests uitgevoerd. Anders blijft de taak onvolledig/geblokkeerd en wordt dat expliciet gemeld.
- `extern/`, submoduleverwijzingen en gebruikerswijzigingen zijn behouden.
- De diff bevat geen ongebruikte tijdelijke code, onbedoelde bestanden of door Codex gemaakte/gewijzigde binaries. Bestaande en door de gebruiker aangeleverde binaries zijn behouden volgens de opdracht.
- De status in `TASK.md` weerspiegelt de werkelijk voltooide werkzaamheden.

Houd de eindrapportage bondig: voltooide stap en gewijzigd gedrag, belangrijkste bestanden, uitgevoerde validatie met resultaat, en concrete resterende problemen. Geef geen lange code-dump in de chat; de leesbare code en diff vormen de oplevering.
