#include "ContentBlockerContract.h"

#include "QmlContract.h"

namespace omaweb {

QStringList validateEngineBlockerContract(const QObject &blocker)
{
    using contract::Method;
    static constexpr Method requiredMethods[] = {
        // The view says which document it is showing and which load of it this
        // is. Content blocking owns the Refusal tally that follows from that,
        // so the view carries no count of its own (ADR 0037).
        {"showPage", false, 4},
        // What a page may not open. Refused where the request arrives, which is
        // the view, so the answer comes back rather than being applied here.
        {"shouldBlockPopup", false, 3},
        // The rules that hide what the page did load: a stylesheet and the
        // scriptlets for one address, and the generic rules a survey of the
        // page's own classes and ids narrows down.
        {"cosmeticStyleSheet", false, 1},
        {"scriptletSource", false, 1},
        {"cosmeticSurveyWanted", false, 1},
        {"genericCosmeticStyleSheet", false, 3},
        // A compiled rule set has been replaced, or the reader has turned
        // blocking off for a site. Either makes the cosmetic rules in force
        // different, so the view asks for them again.
        {"rulesChanged", true, 0},
        {"configurationChanged", true, 0},
    };
    return contract::missing(blocker, {}, requiredMethods);
}

QStringList validateChromeBlockerContract(const QObject &blocker)
{
    using contract::Method;
    using contract::Property;
    static constexpr Property requiredProperties[] = {
        {"refusalTallyGeneration", QMetaType::Int},
    };
    static constexpr Method requiredMethods[] = {
        // The tally for one page address in one Space. A call rather than a
        // property, because it is keyed; the generation above is what makes a
        // binding on it answer again when a tally moves.
        {"refusalTally", false, 2},
        // The per-site switch the tally is stated beside. Keyed per host, not
        // per page address: they are different keys and conflating them would
        // make one of them wrong (ADR 0037).
        {"siteEnabled", false, 1},
        {"setSiteEnabled", false, 2},
    };
    return contract::missing(blocker, requiredProperties, requiredMethods);
}

} // namespace omaweb
