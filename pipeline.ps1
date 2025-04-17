
$exampleFolder = "Exempel"
$exe           = "./desktop.exe"
$python        = "python"

$fileList = @()

if (Test-Path -Path $exampleFolder -PathType Container) {

    $fileList = Get-ChildItem -Path $exampleFolder -File | Select-Object -ExpandProperty Name

    if ($fileList.Count -gt 0) {

        Write-Host "Examples to run found: $($fileList.Count)"

        foreach ($fileName in $fileList) {

            Write-Host "Running desktop.exe on"$fileName""
            $argumentPath = "$exampleFolder" + "/" + "$fileName"

            Write-Host "Attempting to run: $exe "$argumentPath""

            # run desktop.exe
            try {
                Start-Process -FilePath $exe -ArgumentList "$argumentPath" -Wait
            } catch {
                Write-Error "Error running '$exe' for file '$argumentPath': $_"
            }


            # run generate_graph.py 
            $outputname = $fileName -replace "\.txt$", ""
            $outputpath = "Graphs\" + $outputname
            Write-Host $outputpath
            try {
                Start-Process -FilePath $python -ArgumentList "test.py", "$outputpath", "connections.txt", "timestamps.txt", "$argumentPath" -Wait
            } catch {
                Write-Error "Error generating graph: $_"
            }

        }
    } else {
        Write-Host "No files found in the specified folder."
    }

} else {
    Write-Error "Error: There does not seem to exist an examples folder!"
}


