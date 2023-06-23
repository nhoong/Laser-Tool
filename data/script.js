var distance = 0;
var offset = 0;
setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      distance = this.responseText;
      document.getElementById("distance").innerHTML = (distance - offset).toFixed(3);
      console.log("distance",this.responseText);
      document.getElementById('error').style.display='none';
    }
  };

  xhttp.open("GET", "/distance", true)
  xhttp.onerror = function () {
    console.log("*** Not connected to sensor! ***");
    document.getElementById('error').style.display='block';
  }
  xhttp.send();
 
}, 1000 ) ;

/* setInterval(function ( ) {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("offset").innerHTML = this.responseText;
      console.log("offset",this.responseText);
    }
  };
  xhttp.open("GET", "/offset", true);
  xhttp.send();
}, 1000 ) ;
 */
function zero() {
  offset = 0;
  offset = Number(distance);
  document.getElementById("offset").innerHTML = offset.toFixed(3);
  console.log("offset", offset);
} ;

function reset() {
offset = 0;
document.getElementById("offset").innerHTML = offset.toFixed(3);
console.log("offset", offset);
};