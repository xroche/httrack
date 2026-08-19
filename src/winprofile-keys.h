/* Generated from winprofile-keys.tsv by tools/gen-winprofile-keys.py.
 * Do not edit. */
#ifndef WINPROFILE_KEYS_H
#define WINPROFILE_KEYS_H

typedef struct {
  const char *key;
  const char *owners;
  const char *scope;
  const char *kind;
  const char *flag_on;
  const char *flag_off;
  const char *composed_with;
  const char *default_state;
  const char *default_value;
  const char *empty_means;
  const char *legacy_of;
} winprofile_key_t;

#define WINPROFILE_KEY_COUNT 106

/* clang-format off */
static const winprofile_key_t winprofile_keys[WINPROFILE_KEY_COUNT] = {
  {"AcceptLanguage", "win,droid", "setting", "string", "", "", "", "derived", "", "literal", ""},
  {"Build", "win,web,droid", "setting", "list:0:15", "", "", "BuildString", "agreed", "0", "", ""},
  {"BuildString", "win,web,droid", "setting", "string", "", "", "Build", "agreed", "%h%p/%n%q.%t", "literal", ""},
  {"Cache", "win,web,droid", "setting", "checkbox", "", "--cache=0", "", "agreed", "1", "", ""},
  {"Category", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Changes", "win,web,droid", "setting", "checkbox", "--changes", "", "", "agreed", "0", "", ""},
  {"CheckType", "win,web,droid", "setting", "list:0:3", "", "", "", "agreed", "1", "", ""},
  {"Cookies", "win,web,droid", "setting", "checkbox", "", "--cookies=0", "", "agreed", "1", "", ""},
  {"CookiesFile", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"CurrentAction", "win,web,droid", "project", "list:0:7", "", "", "", "agreed", "0", "", ""},
  {"CurrentPath1", "", "read_only", "string", "", "", "", "none", "", "", ""},
  {"CurrentPath2", "", "read_only", "string", "", "", "", "none", "", "", ""},
  {"CurrentURLList", "win,web", "project", "string", "", "", "", "agreed", "", "absent", ""},
  {"CurrentUrl", "win,web,droid", "project", "string", "", "", "", "agreed", "", "absent", ""},
  {"Debugging", "droid", "setting", "string", "", "", "", "none", "", "", ""},
  {"DefaultReferer", "win,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Depth", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Dos", "win,web,droid", "setting", "bitmask", "", "", "", "agreed", "0", "", ""},
  {"ExtDepth", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"FollowRobotsTxt", "win,web,droid", "setting", "list:0:3", "", "", "", "agreed", "2", "", ""},
  {"Footer", "win,web,droid", "setting", "string", "", "", "", "agreed", "<!-- Mirrored from {url} by HTTrack Website Copier/3.x [XR&CO], {date} -->", "literal", ""},
  {"GlobalTravel", "win,web,droid", "setting", "list:0:4", "", "", "", "agreed", "0", "", ""},
  {"HTMLFirst", "win,web,droid", "setting", "checkbox", "--priority=7", "", "", "agreed", "0", "", ""},
  {"HTTP10", "win,web,droid", "setting", "checkbox", "--http-10", "", "", "agreed", "0", "", ""},
  {"HostAlias", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Index", "win,web,droid", "setting", "checkbox", "", "--index=0", "", "agreed", "1", "", ""},
  {"KeepAlive", "win,web,droid", "setting", "checkbox", "--keep-alive", "", "", "agreed", "1", "", ""},
  {"KeepQueryOrder", "win,web,droid", "setting", "checkbox", "--keep-query-order", "", "", "agreed", "0", "", ""},
  {"KeepSlashes", "win,web,droid", "setting", "checkbox", "--keep-double-slashes", "", "", "agreed", "0", "", ""},
  {"KeepWww", "win,web,droid", "setting", "checkbox", "--keep-www-prefix", "", "", "agreed", "0", "", ""},
  {"Log", "win,web,droid", "setting", "checkbox", "--single-log", "", "", "agreed", "1", "", ""},
  {"LogType", "win,web,droid", "setting", "list:0:3", "", "", "", "agreed", "0", "", ""},
  {"MIMEDefsExt1", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt2", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt3", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt4", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt5", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt6", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt7", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsExt8", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime1", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime2", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime3", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime4", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime5", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime6", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime7", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MIMEDefsMime8", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MailIndex", "win,droid", "setting", "checkbox", "", "", "", "agreed", "0", "", ""},
  {"MaxAll", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxConn", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxHtml", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxLinks", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxOther", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxRate", "win,web,droid", "setting", "number", "", "", "", "none", "", "absent", ""},
  {"MaxTime", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"MaxWait", "win,web", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Near", "win,web,droid", "setting", "checkbox", "--near", "", "", "agreed", "0", "", ""},
  {"NoErrorPages", "win,web,droid", "setting", "checkbox", "--generate-errors=0", "--generate-errors", "", "agreed", "0", "", ""},
  {"NoExternalPages", "win,web,droid", "setting", "checkbox", "--replace-external", "", "", "agreed", "0", "", ""},
  {"NoPurgeOldFiles", "win,web,droid", "setting", "checkbox", "--purge-old=0", "--purge-old=1", "", "agreed", "0", "", ""},
  {"NoPwdInPages", "win,web,droid", "setting", "checkbox", "--disable-passwords", "", "", "agreed", "0", "", ""},
  {"NoQueryStrings", "win,web,droid", "setting", "checkbox", "--include-query-string=0", "--include-query-string=1", "", "agreed", "0", "", ""},
  {"NoRecatch", "win,web,droid", "setting", "checkbox", "--do-not-recatch", "", "", "agreed", "0", "", ""},
  {"OtherHeaders", "win,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"ParseAll", "win,web,droid", "setting", "checkbox", "--extended-parsing", "", "", "agreed", "1", "", ""},
  {"ParseJava", "win,web,droid", "setting", "checkbox", "", "--parse-java=0", "", "agreed", "1", "", ""},
  {"PauseFiles", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Port", "win,web,droid", "setting", "string", "", "", "ProxyType,Proxy", "agreed", "", "absent", ""},
  {"PrimaryScan", "win,web,droid", "setting", "list:0:5", "", "", "", "agreed", "3", "", ""},
  {"ProfileFormat", "win,web", "write_only", "number", "", "", "", "none", "", "absent", ""},
  {"ProjectName", "droid", "setting", "string", "", "", "", "none", "", "", ""},
  {"Proxy", "win,web,droid", "setting", "string", "", "", "ProxyType,Port", "agreed", "", "absent", ""},
  {"ProxyType", "win,web,droid", "setting", "list:0:3", "socks5", "", "Proxy,Port", "agreed", "0", "absent", ""},
  {"RateOut", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"RemoveRateout", "win,web,droid", "setting", "checkbox", "--host-control=2", "", "", "agreed", "0", "", ""},
  {"RemoveTimeout", "win,web,droid", "setting", "checkbox", "--host-control=1", "", "", "agreed", "0", "", ""},
  {"Retry", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"RewriteLinks", "win,web,droid", "setting", "list:0:4", "", "", "", "agreed", "0", "", ""},
  {"SingleFile", "win,web,droid", "setting", "checkbox", "--single-file", "", "SingleFileMaxSize", "agreed", "0", "", ""},
  {"SingleFileMaxSize", "win,web,droid", "setting", "string", "", "", "SingleFile", "agreed", "", "absent", ""},
  {"Sitemap", "win,web,droid", "setting", "checkbox", "--sitemap", "", "SitemapUrl", "agreed", "0", "", ""},
  {"SitemapUrl", "win,web,droid", "setting", "string", "", "", "Sitemap", "agreed", "", "absent", ""},
  {"Sockets", "win,web,droid", "setting", "number", "", "", "", "none", "", "absent", ""},
  {"StoreAllInCache", "win,web,droid", "setting", "checkbox", "--store-all-in-cache", "", "", "agreed", "0", "", ""},
  {"StripQuery", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"Test", "win,web,droid", "setting", "checkbox", "--test", "", "", "agreed", "0", "", ""},
  {"TimeOut", "win,web,droid", "setting", "string", "", "", "", "agreed", "", "absent", ""},
  {"TolerantRequests", "win,web,droid", "setting", "checkbox", "--tolerant", "", "", "agreed", "0", "", ""},
  {"Travel", "win,web,droid", "setting", "list:0:4", "", "", "", "agreed", "1", "", ""},
  {"URLHack", "win,web,droid", "setting", "checkbox", "--urlhack", "--urlhack=0", "", "agreed", "1", "", ""},
  {"UpdateHack", "win,web,droid", "setting", "checkbox", "--updatehack", "", "", "agreed", "1", "", ""},
  {"UseHTTPProxyForFTP", "win,web,droid", "setting", "checkbox", "--httpproxy-ftp", "--httpproxy-ftp=0", "", "agreed", "1", "", ""},
  {"UserID", "win,web,droid", "setting", "string", "", "", "", "derived", "", "literal", ""},
  {"Wacz", "win,web", "setting", "checkbox", "--wacz", "", "", "agreed", "0", "", ""},
  {"Warc", "win,web,droid", "setting", "checkbox", "--warc", "", "WarcFile,WarcMaxSize,WarcCdx", "agreed", "0", "", ""},
  {"WarcCdx", "win,web", "setting", "checkbox", "--warc-cdx", "", "Warc", "agreed", "0", "", ""},
  {"WarcFile", "web,droid", "setting", "string", "", "", "Warc", "none", "", "", ""},
  {"WarcMaxSize", "web", "setting", "string", "", "", "Warc", "none", "", "", ""},
  {"WildCardFilters", "win,web,droid", "setting", "string", "", "", "", "agreed", "+*.png +*.gif +*.jpg +*.jpeg +*.css +*.js -ad.doubleclick.net/* -mime:application/foobar", "literal", ""},
  {"WordIndex", "win,web,droid", "setting", "checkbox", "--search-index", "--search-index=0", "", "agreed", "0", "", ""},
  {"KeepDoubleSlashes", "", "setting", "string", "", "", "", "", "", "", "KeepSlashes"},
  {"KeepWwwPrefix", "", "setting", "string", "", "", "", "", "", "", "KeepWww"},
  {"Pause", "", "setting", "string", "", "", "", "", "", "", "PauseFiles"},
  {"ProxyProtocol", "", "setting", "string", "", "", "", "", "", "", "ProxyType"},
  {"Iso9660", "", "read_only", "checkbox", "", "", "", "none", "", "", "Dos"},
};
/* clang-format on */

#endif
