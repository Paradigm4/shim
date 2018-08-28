var baseURL = 'http://localhost:8080';

var sessionQue = undefined;
var sessionIns = undefined;

var tableQue = document.getElementById('que_body');
var tableIns = document.getElementById('ins_body');

var statusQue = document.getElementById('que_foot');
var statusIns = document.getElementById('ins_foot');

statusQue.textContent = 'Loading...';
statusIns.textContent = 'Loading...';


function setDisabledClass(className, value) {
    [].forEach.call(
        document.getElementsByClassName(className),
        function(el) {
            el.disabled = value;
        });
}

function shimRequest(sessionID, endpoint, args, status, requestHandler) {
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

    request.addEventListener(
        'load',
        function() {
            requestHandler(this.status, this.responseText);
        });

    request.addEventListener(
        'error',
        function() {
            status.textContent = 'Shim connection error!';
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

function shimQuery(
    sessionID, query, status, table, postProcess, finalize) {
    status.textContent = 'Loading...';
    shimRequest(
        sessionID,
        'execute_query',
        'query=' + query + '&save=csv%2B',
        status,
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                status.textContent = 'Shim error! ' + code;
                return;
            }

            shimRequest(
                sessionID,
                'read_lines',
                undefined,
                status,
                function(code, data) {
                    console.log(data);
                    if (code !== 200) {
                        status.textContent = 'Shim error! ' + code;
                        return;
                    }

                    populateTable(parseResult(data), table, postProcess);
                    status.textContent = 'Ready.';
                    if (typeof finalize !== 'undefined')
                        finalize();
                });
        });
}

function getQueries() {
    setDisabledClass('que', true);
    shimQuery(
        sessionQue,
        "filter(list('queries'),query_string<>'')",
        statusQue,
        tableQue,
        function(tr) {
            var qid = tr.childNodes[2].textContent;

            var inp = document.createElement('input');
            inp.value = 'Cancel';
            inp.type = 'button';
            inp.onclick = function() {cancelQuery(qid);};
            inp.classList.add('que');

            var td = document.createElement('td');
            td.appendChild(inp);

            tr.insertBefore(td, tr.firstChild);
            return tr;
        },
        function() {
            setDisabledClass('que', false);
        });
}

function getInstances() {
    shimQuery(
        sessionIns,
        "list('instances')",
        statusIns,
        tableIns,
        undefined,
        undefined);
}

function cancelQuery(qid) {
    setDisabledClass('que', true);
    statusQue.textContent = 'Cancelling...';
    shimRequest(
        sessionQue,
        'execute_query',
        "query=cancel('" + qid + "')",
        statusQue,
        function(code, data) {
            console.log(data);
            if (code === 200 ||
                code === 406 &&
                data.indexOf('Query ' + qid + ' not found') !== -1) {
                statusQue.textContent = 'Ready.';
                setDisabledClass('que', false);
                getQueries();
            } else {
                statusQue.textContent = 'Shim error! ' + code;
                return;
            }});
}

function initialize() {
    shimRequest(
        undefined,
        'new_session',
        'admin=1',
        statusQue,
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusQue.textContent = 'Shim error! ' + code;
                return;
            }

            sessionID = data;
            console.log('Session ID: ' + sessionID);
            sessionQue = sessionID;
            getQueries();
        });

    shimRequest(
        undefined,
        'new_session',
        undefined,
        statusIns,
        function(code, data) {
            console.log(data);
            if (code !== 200) {
                statusIns.textContent = 'Shim error! ' + code;
                return;
            }

            sessionID = data;
            console.log('Session ID: ' + sessionID);
            sessionIns = sessionID;
            getInstances();
            document.getElementById('ins_button').disabled=false;
        });
}

initialize();
