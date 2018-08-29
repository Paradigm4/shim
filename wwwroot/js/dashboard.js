var baseURL = 'http://localhost:8080';

var sessionID = undefined;

var statusBar = document.getElementById('status_bar');
statusBar.textContent = 'Loading...';

var tableQue = document.getElementById('que_body');
var tableIns = document.getElementById('ins_body');


function setDisabled(value) {
    [].forEach.call(
        document.getElementsByTagName('input'),
        function(el) {
            el.disabled = value;
        });
}

function shimRequest(endpoint, args, requestHandler) {
    var request = new XMLHttpRequest();

    var url = baseURL + '/' + endpoint + '?';
    if (endpoint != 'new_session') {
        url += 'id=' + sessionID + '&';
    }
    if (args) {
        url += args;
    }
    console.log('shimRequest: ' + url);

    request.open('GET', url, true);

    request.addEventListener(
        'load',
        function() {
            requestHandler(this.status, this.responseText);
        });

    request.addEventListener(
        'error',
        function() {
            statusBar.textContent = 'Shim connection error!';
        });

    request.send();
}

function parseResult(data) {
    var lines = data.split(/\n/);
    var records = [];
    lines.forEach(function(line) {
        if (line === '') {
            return;
        }
        console.log(line);
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

function populateTable(results, table, postProcess) {
    while (table.firstChild)
        table.removeChild(table.firstChild);
    results.forEach(function(result) {
        var tr = document.createElement('tr');
        result.forEach(function(value) {
            var td = document.createElement('td');
            td.textContent = value;
            tr.appendChild(td);
        });
        if (typeof postProcess !== 'undefined')
            postProcess(tr);
        table.appendChild(tr);
    });
}

function shimQuery(query, table, postProcess, finalize) {
    statusBar.textContent = 'Loading...';
    shimRequest(
        'execute_query',
        'query=' + query + '&save=csv%2B',
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }

            shimRequest(
                'read_lines',
                undefined,
                function(code, data) {
                    console.log(data);
                    if (code !== 200) {
                        statusBar.textContent = 'Shim error! ' + code;
                        return;
                    }

                    populateTable(parseResult(data), table, postProcess);
                    statusBar.textContent = 'Ready.';
                    if (typeof finalize !== 'undefined')
                        finalize();
                });
        });
}

function getQueries(init=false) {
    setDisabled(true);
    shimQuery(
        "filter(list('queries'),query_string<>'')",
        tableQue,
        function(tr) {
            var qid = tr.childNodes[2].textContent;

            var inp = document.createElement('input');
            inp.value = 'Cancel';
            inp.type = 'button';
            inp.onclick = function() {cancelQuery(qid);};

            var td = document.createElement('td');
            td.appendChild(inp);

            tr.insertBefore(td, tr.firstChild);
            return tr;
        },
        function() {
            setDisabled(false);
            if (init === true) {
                getInstances();
            }
        });
}

function getInstances() {
    setDisabled(true);
    shimQuery(
        "list('instances')",
        tableIns,
        undefined,
        function() {
            setDisabled(false);
        });
}

function cancelQuery(qid) {
    setDisabled(true);
    statusBar.textContent = 'Cancelling...';
    shimRequest(
        'execute_query',
        "query=cancel('" + qid + "')",
        function(code, data) {
            console.log(data);
            if (code === 200 ||
                code === 406 &&
                data.indexOf('Query ' + qid + ' not found') !== -1) {
                statusBar.textContent = 'Ready.';
                setDisabled(false);
                getQueries();
            } else {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }});
}

function initialize() {
    shimRequest(
        'new_session',
        'admin=1',
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusBar.textContent = 'Shim error! ' + code;
                return;
            }

            sessionID = data;
            console.log('Session ID: ' + sessionID);
            getQueries(true);
        });
}

initialize();
