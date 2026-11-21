import QtQuick
import LearningBox

// Everything the app can tell about the word (or sentence) that stands in a text box: its article, the
// singular forms of each gender, the plural(s) and example sentences to choose from - and for a verb its
// Partizip II (with haben / sein) and a simple example sentence with it.
// ONE implementation for all three ways a word gets into the app: typed in the Dictionary, typed in Add card,
// or picked in Lens (Lens opens Add card with the word). Show the result with WordDetails.
//
//   WordLookup { id: info; word: field.text; onCapitalise: text => field.text = text }
//   WordDetails { info: info }
Item {
    id: root
    visible: false
    width: 0
    height: 0

    // ---- in ----
    property string word: ""            // the text in the box
    property bool active: true          // false: no lookups (e.g. editing an existing card)
    property string meaning: Translator.meaningLanguage // language of the example translations
    property bool autoPick: true        // choose the first example sentence on its own
    // Language of the word. The learning box's, unless the user of this item says otherwise (the Dictionary looks
    // German words up as German, whichever learning box is selected).
    property string lang: CardStore.learningLanguage

    // The word should stand in the box with a capital first letter (German nouns, first letter of a sentence)
    signal capitalise(string text)
    // An example sentence was chosen (byUser) or chosen automatically; "" = none
    signal exampleChosen(string text, bool byUser)

    // ---- the word ----
    readonly property var parts: /^(?:(der|die|das)\s+)?(\S+)$/i.exec(word.trim()) // one word, maybe with article
    readonly property string bare: parts ? parts[2] : ""
    // A separable verb typed as a phrase ("füge hinzu"): looked up under its infinitive ("hinzufügen"), see Translator
    readonly property string separableKey: lang === "de" && !parts ? Translator.separablePhrase(word) : ""

    // ---- article and forms (word pack offline, Wiktionary online, saved for offline use) ----
    property string grammarFor: ""
    property int grammarRequest: -1
    property string wikiFront: ""
    property string wikiForms: ""
    property string packFront: ""
    property string packForms: ""
    // Two words with one spelling (Fahren / fahren) and the verb chosen (lowercase): the word pack's noun entry
    // ("das Fahren") is not this word, so no article, no noun forms and no capital letter
    readonly property bool verbChosen: ambiguous && !asNoun
    readonly property string front: verbChosen ? "" : (packFront !== "" ? packFront : wikiFront)   // "der Kellner"
    readonly property string forms: wikiForms !== "" ? wikiForms : (verbChosen ? "" : packForms)    // "Plural ...", "Singular die ..."
    // A verb has "forms" too (the "Partizip II ..." line) but is no noun: it keeps its lowercase letter
    readonly property bool isNoun: front !== "" || wikiForms.split("\n").some(l => l.trim() !== "" && !/^\s*Partizip II(\s|$)/.test(l))
    readonly property bool isVerb: wikiForms.split("\n").some(l => /^\s*Partizip II(\s|$)/.test(l))

    // ---- one spelling, two words ----
    // "Berichten" is the plural of the noun "Bericht" (reports) and also the verb "berichten" (to report).
    // A word typed with a capital that is a noun, while Wiktionary also has a page for its lowercase spelling,
    // could be either. The user picks (WordDetails shows the two choices): the choice is the capital letter,
    // as in German spelling, so card front, meaning, forms and example sentences all follow the same word.
    property string senseKey: ""        // lowercase spelling whose two words are on offer, "" = none
    property string senseNoun: ""       // the noun side, e.g. "der Bericht"
    property string pageFor: ""
    property int pageRequest: -1
    readonly property bool asNoun: !!parts && /^[A-ZÄÖÜ]/.test(bare)
    readonly property bool ambiguous: lang === "de" && !!parts && !parts[1] && senseKey !== ""
                                      && senseKey === bare.toLowerCase()
    // The text to translate for the meaning: for the noun side of an ambiguous spelling the singular with its
    // article ("der Bericht"), which no translator reads as a verb; otherwise the word as typed.
    readonly property string meaningText: ambiguous && asNoun && senseNoun !== "" ? senseNoun : word.trim()
    // Translators answer "der Bericht" with "the report": drop the English article again
    function meaningFix(text) {
        return meaningText !== word.trim() ? String(text).replace(/^(?:the|a|an)\s+/i, "") : text
    }
    function chooseSense(noun) {
        if (!ambiguous || !parts)
            return
        const w = parts[2]
        const nw = (noun ? w[0].toUpperCase() : w[0].toLowerCase()) + w.slice(1)
        if (nw === w)
            return
        const i = word.lastIndexOf(w)
        capitalise(word.slice(0, i) + nw + word.slice(i + w.length))
    }
    function probeSense() {
        if (lang !== "de" || !parts || parts[1] || !asNoun || !isNoun)
            return
        const k = bare.toLowerCase()
        if (senseKey === k || pageFor === k)
            return
        pageFor = k
        pageRequest = Translator.lookupPage(k)
    }
    // Example sentences that fit the chosen word: a capitalised form inside a sentence is the noun, any other use
    // (lowercase, or a capital only because it starts the sentence) is the verb. Unfiltered when nothing fits.
    function occursInside(t, form) {
        let i = -1
        while ((i = t.indexOf(form, i + 1)) >= 0) {
            if (/[A-Za-zÄÖÜäöüß]/.test(t.charAt(i + form.length)) || (i > 0 && /[A-Za-zÄÖÜäöüß]/.test(t.charAt(i - 1))))
                continue
            const before = t.slice(0, i)
            if (before.trim() === "" || /[.!?:\u201E"\u00BB]\s*$/.test(before))
                continue // the capital may only be the start of the sentence
            return true
        }
        return false
    }
    function senseFilter(list) {
        if (!ambiguous)
            return list
        const cap = senseKey[0].toUpperCase() + senseKey.slice(1)
        const wantNoun = asNoun
        const fit = list.filter(e => occursInside(e.text, cap) === wantNoun
                                     && (wantNoun || e.text.toLowerCase().indexOf(senseKey) >= 0))
        return fit.length > 0 ? fit : list
    }

    // Rows of the unified layout: [{kind, text, mark, example}] - singular of each gender, plural, plural of each gender;
    // for a verb the one row {kind: "participle", text: "ist gegangen", example: "Ich bin gegangen."}.
    // `mark` is the article that gives the colour mark (der blue, die red, das green): the gender is never written.
    // The lines come in the card's order: own plural, then per other gender "Singular der ..." and its "Plural ...".
    // Old cards: "Pl. ...", "Sg. ...", "Mask. der ...", "Mask. Pl. ...", "Fem. ...".
    readonly property var rows: {
        const article = t => (/^(der|die|das)\s/i.exec(t) ?? ["", ""])[1].toLowerCase()
        const noDie = t => t.replace(/(^|\/\s*)die\s+/gi, "$1") // plurals are shown without "die"
        const canon = forms.split("\n").map(l => l.trim()).filter(l => l !== "").map(l =>
            l.replace(/^(?:Mask|Fem)\.\s+(?:Pl\.|Plural)\s+/, "Plural ")
             .replace(/^(?:Mask|Fem)\.\s+/, "Singular ")
             .replace(/^Pl\.\s+/, "Plural ").replace(/^Sg\.\s+/, "Singular "))
        let ownSg = front, ownPl = "", cur = ""
        const sg = ({}), pl = ({})
        let verbRow = null
        for (const l of canon) {
            const pm = /^Partizip II\s+(.+)$/.exec(l)
            if (pm) { // "Partizip II ist gegangen · Ich bin gegangen."
                const parts = pm[1].split(/\s+\u00b7\s+/)
                verbRow = { kind: "participle", text: parts[0].trim(), mark: "", example: (parts[1] ?? "").trim(),
                            infinitive: (parts[2] ?? "").trim() }
                continue
            }
            const m = /^(Singular|Plural)\s+(.+)$/.exec(l)
            if (!m)
                continue
            if (m[1] === "Singular") {
                if (ownSg === "") { ownSg = m[2]; cur = "own"; continue } // the word is a plural form: its singular
                cur = article(m[2])
                sg[cur] = m[2]
            } else if (cur === "" || cur === "own") {
                ownPl = m[2]
            } else {
                pl[cur] = m[2]
            }
        }
        const g = article(ownSg)
        const out = []
        const add = (kind, text, mark) => { if (text) out.push({ kind: kind, text: text, mark: mark }) }
        add("singular", g === "der" ? ownSg : (sg["der"] ?? ""), "der ")
        add("singular", g === "die" ? ownSg : (sg["die"] ?? ""), "die ")
        add("singular", g === "das" ? ownSg : (sg["das"] ?? ""), "das ")
        add("plural", noDie(ownPl), g !== "" ? g + " " : "")
        add("plural", noDie(pl["der"] ?? ""), "der ")
        add("plural", noDie(pl["die"] ?? ""), "die ")
        if (verbRow && verbRow.text !== "")
            out.push(verbRow)
        return out
    }

    function requestGrammar() {
        const sep = separableKey
        const w = sep !== "" ? sep : bare
        const k = w.toLowerCase()
        if (k !== senseKey)
            senseKey = ""
        if (k !== pageFor)
            pageFor = ""
        if (w === grammarFor)
            return
        grammarFor = w
        grammarRequest = -1
        wikiFront = ""; wikiForms = ""; packFront = ""; packForms = ""; wikiExamples = []
        if (w === "" || lang !== "de")
            return
        if (sep !== "") {
            grammarRequest = Translator.lookupSeparable(sep)
            return
        }
        const known = WordPacks.lookup(w)
        if (known.front && /^(der|die|das)\s/i.test(known.front))
            packFront = known.front
        packForms = (known.back ?? "").replace(/\s*·\s*Pl\./, "\nPl.").split("\n")
                        .filter(l => /^\s*Pl\./.test(l))
                        .map(l => l.replace(/^\s*Pl\.\s*/, "Plural ").replace(/(^|\/\s*|Plural\s+)die\s+/gi, "$1"))
                        .join("\n")
        applyCapital()
        if (w.length >= 2) {
            // The article typed (or the pack's) picks the right entry on a page with several: der Reis / die Reise
            // (not the pack's article when the verb of an ambiguous spelling is meant: "das Fahren" would make
            // this a noun lookup and "fahren" would never be read as the verb with its Partizip II)
            const art = parts && parts[1] ? parts[1].toLowerCase()
                      : (packFront !== "" && !verbChosen ? packFront.split(" ")[0].toLowerCase() : "")
            console.log("LB-qml grammar request:", art !== "" ? art + " " + w : w, "verbChosen:", verbChosen, "ambiguous:", ambiguous)
            grammarRequest = Translator.lookupGrammar(art !== "" ? art + " " + w : w) // lowercase words: only nouns get an article
        }
    }

    // The text with a capital first letter, or "" when it is fine as it is. A single word only gets the capital
    // when it is a noun ("gehen" stays), the first letter of a sentence always does.
    function capitalised() {
        if (lang !== "de")
            return ""
        const t = word
        if (parts) {
            const w = parts[2]
            if (!/^[a-zäöü]/.test(w) || !isNoun || grammarFor !== w || verbChosen)
                return ""
            const i = t.lastIndexOf(w)
            return t.slice(0, i) + w[0].toUpperCase() + w.slice(1) + t.slice(i + w.length)
        }
        return /^\s*[a-zäöü]/.test(t)
                ? t.replace(/^(\s*)([a-zäöü])/, (x, a, b) => a + b.toUpperCase()) : ""
    }
    function applyCapital() {
        const t = capitalised()
        if (t !== "")
            capitalise(t)
    }

    // ---- example sentences: word pack (offline) + Tatoeba (online) ----
    property var examples: []           // [{text, translation}]
    property int examplesRequest: -1
    property var picked: []             // indices of the sentences that go on the card (several allowed), [] = none
    property bool deselected: false
    property string examplesFor: ""
    property bool examplesAsked: false  // an online search for sentences was started for this word
    property string examplesError: ""   // why the last online search failed ("" = it worked or none yet)
    // The chosen sentences, one per line: the card keeps them all in its example field
    readonly property string chosen: picked.filter(i => i < examples.length).map(i => examples[i].text).join("\n")

    function isPicked(i) { return picked.indexOf(i) >= 0 }

    // A verb with no sentence for its infinitive (separable verbs are mostly split: "Ich ziehe mich an"): sentences
    // with its Partizip II instead ("Sie hat sich warm angezogen."), which also shows the Perfekt.
    property string participleTried: ""
    // Example sentences of the verb's own Wiktionary page: used when Tatoeba has none (they show the conjugated
    // forms: "Ich ziehe mich an.")
    property var wikiExamples: []
    function useWikiExamples() {
        if (examples.length > 0 || wikiExamples.length === 0 || examplesRequest >= 0 || examplesFor === "")
            return false
        examples = wikiExamples.map(t => ({ text: t, translation: "" }))
        if (picked.length === 0 && autoPick && !deselected)
            pickAll([0], false)
        return true
    }
    function fallbackExamples() {
        if (!useWikiExamples())
            participleExamples()
    }
    readonly property string participleForm: {
        const r = rows.find(x => x.kind === "participle")
        if (!r)
            return ""
        const w = r.text.split(/\s*\/\s*/)[0].trim().split(/\s+/)
        return w[w.length - 1] ?? ""
    }
    function participleExamples() {
        const p = participleForm
        if (p === "" || lang !== "de" || examples.length > 0 || examplesRequest >= 0 || participleTried === p
                || !Translator.useOnline || examplesFor === "")
            return
        participleTried = p
        console.log("LB-qml examples via participle:", p)
        examplesRequest = Translator.suggestExamplesFor(p, lang, meaning)
    }

    // Choose these sentences (ascending indices); byUser: ticked by hand, false: chosen automatically
    function pickAll(list, byUser) {
        const fresh = list.filter(i => picked.indexOf(i) < 0)
        picked = list
        deselected = list.length === 0
        for (const i of fresh) {
            const e = examples[i]
            if (e && e.translation) // in the meaning language: the examples are asked for / filtered that way
                Translator.rememberBetween(lang, meaning, e.text, e.translation)
        }
        exampleChosen(chosen, byUser === true)
    }
    // CheckBox: tick / untick one sentence
    function toggle(i) {
        pickAll(isPicked(i) ? picked.filter(k => k !== i) : picked.concat([i]).sort((a, b) => a - b), true)
    }

    function findExamples(force) {
        const t = word.trim()
        const q = parts ? parts[2] : t
        const key = q + "|" + meaning + "|" + lang + "|" + (ambiguous ? "2" : "")
        if (!force && key === examplesFor)
            return
        examplesFor = key
        participleTried = ""
        examplesAsked = false
        examplesError = ""
        examples = []
        examplesRequest = -1
        picked = []
        deselected = false
        if (autoPick)
            exampleChosen("", false)
        if (q.length < 2 || t.split(/\s+/).length > 3)
            return
        const persian = meaning === "fa"
        examples = lang !== "de" ? []
                 : senseFilter(WordPacks.examplesContaining(q, 3).map(e => ({ text: e.text, translation: persian ? e.translation : "" })))
        if (examples.length > 0 && autoPick)
            pickAll([0], false) // the first one is ticked to start with; tick more or untick it
        console.log("LB-qml examples for:", q, "pack:", examples.length, "online:", Translator.useOnline, "active:", active)
        if (Translator.useOnline) {
            examplesAsked = true
            examplesRequest = Translator.suggestExamplesFor(q, lang, meaning)
        }
    }

    // ---- when ----
    function refresh(force) {
        if (!active)
            return
        requestGrammar()
        findExamples(force === true)
        if (!parts)
            applyCapital() // a sentence
    }
    // Right now (Enter, a tapped suggestion, a changed setting) instead of after the typing pause
    function refreshNow() { debounce.stop(); refresh(true) }

    Timer {
        id: debounce
        interval: 600 // wait until typing pauses
        onTriggered: root.refresh(false)
    }
    onWordChanged: {
        if (!active)
            return
        if (word.trim() === "") {
            debounce.stop()
            refresh(false)
        } else {
            debounce.restart()
        }
    }
    onActiveChanged: if (active) debounce.restart()
    onLangChanged: {
        grammarFor = "" // the article and forms of the word are the other language's now
        if (active)
            debounce.restart()
    }

    Connections {
        target: Translator
        function onGrammarFound(requestId, grammar) {
            if (requestId !== root.grammarRequest)
                return
            root.grammarRequest = -1
            console.log("LB-qml grammar answer:", root.bare, "front:", grammar.front, "forms:", String(grammar.forms ?? "").replace(/\n/g, "|"))
            // Wiktionary says the word is the plural form of another noun ("Reis" = plural of "Real"), but the
            // word pack knows it as a singular noun, or it was typed with "der" / "das": it is the singular noun
            // (rice), not that plural. The pack's forms are used then.
            const pluralOfOther = (grammar.front ?? "") === "" && (grammar.lemma ?? "") !== ""
                                  && grammar.lemma.toLowerCase() !== root.bare.toLowerCase()
            const typedSingular = root.parts && /^(der|das)$/i.test(root.parts[1] ?? "")
            if (pluralOfOther && (root.packFront !== "" || typedSingular)) {
                root.wikiFront = ""
                root.wikiForms = ""
                root.applyCapital()
                return
            }
            root.wikiFront = grammar.front ?? ""
            root.wikiForms = grammar.forms ?? ""
            root.wikiExamples = grammar.examples ?? []
            root.applyCapital()
            root.probeSense()
            if (root.active)
                root.fallbackExamples()
        }
        function onPageChecked(requestId, exists, error) {
            if (requestId !== root.pageRequest)
                return
            root.pageRequest = -1
            if (!exists || (error ?? "") !== "" || !root.asNoun || !root.isNoun || root.bare.toLowerCase() !== root.pageFor)
                return
            const sg = root.rows.filter(r => r.kind === "singular")
            root.senseNoun = sg.length > 0 ? sg[0].text : root.bare
            root.senseKey = root.pageFor // two words: the user chooses (WordDetails)
            if (root.active)
                root.findExamples(false)
        }
        function onExamplesSuggested(requestId, list, error) {
            if (requestId !== root.examplesRequest)
                return
            root.examplesRequest = -1
            root.examplesError = error ?? ""
            const merged = root.examples.slice()
            const seen = merged.map(e => e.text.toLowerCase())
            for (const e of root.senseFilter(list))
                if (merged.length < 5 && seen.indexOf(e.text.toLowerCase()) < 0)
                    merged.push(e)
            root.examples = merged
            if (root.picked.length === 0 && root.autoPick && !root.deselected && merged.length > 0)
                root.pickAll([0], false)
            if (merged.length === 0 && root.active)
                root.fallbackExamples()
        }
    }
}
