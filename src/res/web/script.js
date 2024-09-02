(function() {
  console.log("ready");
})();

function upload(type) {
  var fil = document.getElementById("file-" + type);
  if (fil.files.length == 0) {
    alert("No file selected!");
  } else {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = () => {
      if (xhr.readyState === XMLHttpRequest.DONE) {
        if (xhr.status === 200) {
          // Assuming the server returns a success message or HTML
          document.open();
          document.write(xhr.responseText);
          document.close();
        } else if (xhr.status === 0) {
          alert("Server closed the connection!");
          location.reload();
        } else {
          alert(xhr.status + " error!\r\n" + xhr.responseText);
          location.reload();
        }
      }
    };
    
    xhr.upload.onprogress = (e) => {
      // Update the progress bar for the upload
      const progressElement = document.getElementById("progress-" + type);
      if (e.lengthComputable) {
        const percentComplete = (e.loaded * 100 / e.total).toFixed(0);
        progressElement.textContent = "Uploaded: " + percentComplete + "%";
      }
    };

    // Disable the upload button and file input while uploading
    const uploadButton = document.getElementById("upload-" + type);
    uploadButton.disabled = true;
    fil.disabled = true;

    // Prepare the request for file upload
    xhr.open("POST", "/upload", true);
    xhr.setRequestHeader("x-type", type);
    xhr.setRequestHeader("x-filename", fil.files[0].name);
    xhr.send(fil.files[0]);
  }
}
