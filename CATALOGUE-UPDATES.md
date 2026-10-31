# Catalogue updates (for the next release)

Changes to the feed catalogue, collected while checking the links. They are
applied together in the next update, in both catalogues:

- `src/feeds_cat.c` here (the MSX RSS reader)
- `engine/pva_news_cat.c` in msx-picoplay-picoamp (PicoAmp)

A feed keeps its code, so it stays in My feeds after the update.

| Code | Feed | Now | Change to | Status |
|---|---|---|---|---|
| `tweak` | Tweakers | `https://feeds.feedburner.com/tweakers/mixed` (does not work) | `https://tweakers.net/feeds/mixed.xml` | to do |
| `tweakn` (new) | Tweakers, nieuws | - | `https://tweakers.net/feeds/nieuws.xml` | to do |

## To look into

| Code | Feed | Problem | Idea | Status |
|---|---|---|---|---|
| `spiegel`, `spint` | Der Spiegel, Der Spiegel International | The headlines come, the full articles do not | The old address `http://www.spiegel.de/schlagzeilen/rss/0,5291,676,00.xml` probably leads to the same feed and the same article pages. More likely the article pages themselves are the problem: bot protection (403), a consent page, Spiegel+ (paywalled) text, or their size. The I screen after a failed article shows the HTTP status and error, which tells which one. | to check |
