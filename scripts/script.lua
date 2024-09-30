Speed = 0.1
awala = Transform.new()

---@param dt number
---@return nil
function update(dt)
    transform = getTransform()

    transform.position = transform.position + Vec3.new(0, 0, Speed * dt)
    Speed = Speed + 0.1 * dt
end
