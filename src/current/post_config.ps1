$uri = 'http://10.0.0.190/save?op_mode=1&sta_ssid=Dreadnought&sta_pass=I%40dont%21thinkso&ap_ssid=ESP32-UPS-SETUP-4AE49D&ap_pass=admin&ups_name=ups&nut_user=admin&nut_pass=admin&upstream_host=10.0.0.18&upstream_port=3493&portal_pass='
$creds = [Convert]::ToBase64String([Text.Encoding]::ASCII.GetBytes('admin:admin'))
$headers = @{ Authorization = "Basic $creds" }
$resp = Invoke-WebRequest -Uri $uri -Method GET -Headers $headers -UseBasicParsing
Write-Host "HTTP $($resp.StatusCode)"
