Speed = 0.7
RotationSpeed = 45

---@return nil
function update()
    transform = getTransform()

    if (Input.GetKey(KeyCode.W)) then
        transform:Translate(transform.forward, Speed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.S)) then
        transform:Translate(transform.forward, -Speed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.A)) then
        transform:Rotate(transform.forward, RotationSpeed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.D)) then
        transform:Rotate(transform.forward, RotationSpeed * -Time.deltaTime)
    elseif (Input.GetKey(KeyCode.Q)) then
        transform:Rotate(transform.right, RotationSpeed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.E)) then
        transform:Rotate(transform.right, RotationSpeed * -Time.deltaTime)
    elseif (Input.GetKey(KeyCode.Z)) then
        transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * Time.deltaTime)
    elseif (Input.GetKey(KeyCode.C)) then
        transform:Rotate(Vec3.new(0, 1, 0), RotationSpeed * -Time.deltaTime)
    end
end
