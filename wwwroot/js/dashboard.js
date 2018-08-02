var baseURL = 'http://localhost:8080';

var sessionID = undefined;
var statusElement = document.getElementById('table_foot');

status.textContent = 'Loading...';


function shimRequest(endpoint, args, requestHandler) {
    var request = new XMLHttpRequest();

    var url = baseURL + '/' + endpoint + '?';
    if (endpoint != 'new_session') {
        url += 'id=' + sessionID + '&';
    }
    if (args) {
        url += args;
    }
    // url = encodeURI(url);
    console.log('shimRequest: ' + url);

    request.open('GET', url, true);

    request.onload = function() {
        if (request.status >= 200 && request.status < 400) {
            requestHandler(request.responseText);
        } else {
            status.textContent = 'Shim connection error!';
        }
    };

    request.onerror = function() {
        status.textContent = 'Shim connection error!';
    };

    request.send();
}


function shimList() {
    shimRequest(
        'execute_query',
        "query=filter(list('queries'),query_string<>'')&save=csv%2B",
        function(data) {
            console.log(data);
            shimRequest('read_lines', undefined, function(data) {
                console.log(data);
                parseResult(data)
            });
        });
}

var l;

function parseResult(data) {
    var lines = data.split(/\n/);
    var records = [];
    lines.forEach(function(line) {
        if (line === '') {
            return;
        }
        console.log(line);
        l=line;
        var rec = [];
        var re_token = /(?:^|,)(?:(?:'((?:(?:\\')|[^'])*)')|([^',]*))/g;
        while ((token = re_token.exec(line)) !== null) {
            if (token[2] === undefined) {
                console.log(token[1]);
                rec.push(token[1].replace(/\\/g, ''));
            } else {
                var token_raw = token[2];
                var token_parse = parseFloat(token_raw);
                if (isNaN(token_parse)) {
                    if (token_raw === 'true') {
                        token_parse = true;
                    } else if (token_raw === 'false') {
                        token_parse = false;
                    } else {
                        token_parse = token_raw;
                    }
                }
                rec.push(token_parse);
            }
        }
        console.log(rec);
        records.push(rec);
    });
    return records;
}


shimRequest('new_session', undefined, function(data) {
    sessionID = data;
    console.log('Session ID: ' + sessionID);
    shimList();
});
