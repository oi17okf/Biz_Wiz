
$exampleFolder = "examples"
$exe           = "desktop.exe"
$python        = "graph_generator.py"

$fileList = @()

if (Test-Path -Path $exampleFolder -PathType Container) {

    $fileList = Get-ChildItem -Path $exampleFolder -File | Select-Object -ExpandProperty Name

    if ($fileList.Count -gt 0) {

        Write-Host "Examples to run found: $($fileList.Count)"

        foreach ($fileName in $fileList) {

            Write-Host "Running desktop.exe on " + $fileName
            $argumentPath = Join-Path -Path $exampleFolder -ChildPath $fileName

            Write-Host "Attempting to run: $exePath ""$argumentPath"""

            # run desktop.exe
            try {
                Start-Process -FilePath $exe -ArgumentList "$argumentPath"
            } catch {
                Write-Error "Error running '$exe' for file '$argumentPath': $_"
            }


            # run generate_graph.py 
            $outputname = $file -replace "\.txt$", ""
            try {
                StartProcess -FilePath $python -ArgumentList "outputname" "connections.txt", "timestamps.txt"
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


